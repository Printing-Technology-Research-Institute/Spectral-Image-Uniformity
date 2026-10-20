from __future__ import annotations

import json
import queue
import threading
import time
import traceback
from pathlib import Path

import numpy as np

from raw_line_pipeline import RawLinePipeline
from replay_backend import _wavelengths
from sample_boundary_detector import (
    DetectorConfig,
    LineCalibrator,
)
from simulate_stream_detection import (
    load_sources,
    open_raw,
    resolve_workspace_file,
)
from stream_source import StreamMessage
from stream_visualization import (
    DEFAULT_FALSE_COLOR_WAVELENGTHS_NM,
)


APP_DIR = Path(__file__).resolve().parent
WORKSPACE = APP_DIR.parents[1]
DEFAULT_PROVENANCE = (
    WORKSPACE / "CAE訓練" / "導電材料_RT_v3" / "calibration_provenance.json"
)


class RecordedRawStreamWorker:
    """Replay uncorrected BIL files through the production-style line pipeline."""

    def __init__(
        self,
        *,
        provenance_path: str | Path = DEFAULT_PROVENANCE,
        chunk_lines: int = 20,
        line_rate: float = 250.0,
        gap_lines: int = 64,
        false_color_wavelengths_nm: tuple[float, float, float] = (
            DEFAULT_FALSE_COLOR_WAVELENGTHS_NM
        ),
        preview_min: float = 0.0,
        preview_max: float = 0.9,
        preview_gamma: float = 1.0,
        detector_percentile_low: float = 1.0,
        detector_percentile_high: float = 99.0,
        trim_bands: int = 10,
        clip_max: float = 1.2,
        min_white_column_fraction: float = 0.2,
        material_pixel_threshold: float = 0.08,
        spatial_roi_start: int = 0,
        spatial_roi_end: int | None = None,
        start_threshold: float = 0.20,
        end_threshold: float = 0.04,
        start_confirm_lines: int = 4,
        end_confirm_lines: int = 8,
        min_sample_lines: int = 200,
        max_sample_lines: int = 1200,
        loop: bool = True,
    ) -> None:
        if chunk_lines < 1 or line_rate <= 0 or gap_lines < 1:
            raise ValueError("Invalid recorded-stream timing settings")
        if preview_max <= preview_min:
            raise ValueError("preview_max must be greater than preview_min")
        if len(false_color_wavelengths_nm) != 3:
            raise ValueError("False-color preview requires three wavelengths")
        if preview_gamma <= 0:
            raise ValueError("preview_gamma must be positive")
        if not 0.0 <= detector_percentile_low < detector_percentile_high <= 100.0:
            raise ValueError("Invalid detector contrast percentiles")
        self.provenance_path = Path(provenance_path).resolve()
        self.chunk_lines = int(chunk_lines)
        self.line_rate = float(line_rate)
        self.gap_lines = int(gap_lines)
        self.false_color_wavelengths_nm = tuple(
            float(value) for value in false_color_wavelengths_nm
        )
        self.preview_min = float(preview_min)
        self.preview_max = float(preview_max)
        self.preview_gamma = float(preview_gamma)
        self.detector_percentile_low = float(detector_percentile_low)
        self.detector_percentile_high = float(detector_percentile_high)
        self.loop = bool(loop)
        self._calibration_settings = {
            "trim_bands": trim_bands,
            "clip_max": clip_max,
            "min_white_column_fraction": min_white_column_fraction,
            "material_pixel_threshold": material_pixel_threshold,
            "spatial_roi_start": spatial_roi_start,
            "spatial_roi_end": spatial_roi_end,
        }
        self._detector_config = DetectorConfig(
            material_pixel_threshold=material_pixel_threshold,
            start_threshold=start_threshold,
            end_threshold=end_threshold,
            start_confirm_lines=start_confirm_lines,
            end_confirm_lines=end_confirm_lines,
            min_sample_lines=min_sample_lines,
            max_sample_lines=max_sample_lines,
        )

        self._messages: queue.Queue[StreamMessage] = queue.Queue()
        self._run_event = threading.Event()
        self._run_event.set()
        self._analysis_enabled = threading.Event()
        self._analysis_reset_requested = threading.Event()
        self._stop_event = threading.Event()
        self._thread = threading.Thread(
            target=self._run,
            name="recorded-raw-stream",
            daemon=True,
        )
        self._thread.start()

    def resume(self) -> None:
        self._run_event.set()

    def pause(self) -> None:
        self._run_event.clear()

    def set_analysis_enabled(self, enabled: bool) -> None:
        if enabled:
            self._analysis_enabled.set()
        else:
            self._analysis_enabled.clear()

    def reset_analysis(self) -> None:
        """Reset partial boundary/cube state while preview playback continues."""
        self._analysis_reset_requested.set()

    def request_line_rate(self) -> None:
        self._messages.put(
            StreamMessage(
                kind="line_rate_report",
                line_rate_hz=float(self.line_rate),
                line_rate_min_hz=0.1,
                line_rate_max_hz=100000.0,
                line_rate_control_enabled=True,
            )
        )

    def set_line_rate_hz(self, line_rate_hz: float) -> None:
        value = float(line_rate_hz)
        if not np.isfinite(value) or value <= 0:
            raise ValueError("line_rate_hz must be a positive finite number")
        self.line_rate = value
        self._messages.put(
            StreamMessage(
                kind="line_rate_set",
                line_rate_hz=value,
                line_rate_requested_hz=value,
                line_rate_min_hz=0.1,
                line_rate_max_hz=100000.0,
                line_rate_control_enabled=True,
            )
        )

    def poll(self) -> list[StreamMessage]:
        messages: list[StreamMessage] = []
        while True:
            try:
                messages.append(self._messages.get_nowait())
            except queue.Empty:
                return messages

    def close(self, timeout: float = 2.0) -> None:
        self._stop_event.set()
        self._run_event.set()
        self._thread.join(timeout=timeout)

    def _wait_until_running(self) -> bool:
        while not self._stop_event.is_set():
            if self._run_event.wait(timeout=0.1):
                return True
        return False

    def _pace(self, line_count: int) -> bool:
        remaining = line_count / self.line_rate
        while remaining > 0 and not self._stop_event.is_set():
            if not self._wait_until_running():
                return False
            step = min(remaining, 0.05)
            started = time.monotonic()
            if self._stop_event.wait(step):
                return False
            if self._run_event.is_set():
                remaining -= time.monotonic() - started
        return not self._stop_event.is_set()

    def _process_block(
        self,
        raw_block: np.ndarray,
        *,
        pipeline: RawLinePipeline,
        sample_id: str,
        sample_index: int,
        sample_count: int,
        detect_samples: bool,
    ) -> np.ndarray:
        for message in pipeline.process_block(
            raw_block,
            sample_id=sample_id,
            sample_index=sample_index,
            sample_count=sample_count,
            detect_samples=detect_samples,
        ):
            self._messages.put(message)
        return pipeline.last_scores

    def _run(self) -> None:
        try:
            provenance = json.loads(
                self.provenance_path.read_text(encoding="utf-8")
            )
            sources = load_sources(provenance)
            first = sources[0]
            if any(
                source.bands != first.bands or source.width != first.width
                for source in sources
            ):
                raise ValueError("Every recorded source must share one line shape")
            reference_directory = WORKSPACE / "20260528_導電材料"
            dark_reference = resolve_workspace_file(
                provenance["dark_reference"],
                reference_directory,
            )
            white_reference = resolve_workspace_file(
                provenance["white_reference"],
                reference_directory,
            )
            calibrator = LineCalibrator.from_files(
                dark_reference,
                white_reference,
                bands=first.bands,
                width=first.width,
                **self._calibration_settings,
            )
            pipeline = RawLinePipeline(
                calibrator=calibrator,
                detector_config=self._detector_config,
                false_color_wavelengths_nm=self.false_color_wavelengths_nm,
                preview_min=self.preview_min,
                preview_max=self.preview_max,
                preview_gamma=self.preview_gamma,
                detector_percentile_low=self.detector_percentile_low,
                detector_percentile_high=self.detector_percentile_high,
            )
            analysis_was_enabled = self._analysis_enabled.is_set()

            while not self._stop_event.is_set():
                for source_index, source in enumerate(sources):
                    if not self._wait_until_running():
                        return
                    header = source.hdr_path.read_text(
                        encoding="utf-8",
                        errors="ignore",
                    )
                    wavelengths = _wavelengths(header)
                    pipeline.set_wavelengths(wavelengths)
                    raw = open_raw(source)
                    background_lines: list[np.ndarray] = []
                    for start in range(0, source.lines, self.chunk_lines):
                        if not self._wait_until_running():
                            return
                        stop = min(start + self.chunk_lines, source.lines)
                        raw_block = np.asarray(raw[start:stop])
                        analysis_enabled = self._analysis_enabled.is_set()
                        reset_requested = self._analysis_reset_requested.is_set()
                        if analysis_enabled != analysis_was_enabled or reset_requested:
                            pipeline.reset()
                            analysis_was_enabled = analysis_enabled
                            self._analysis_reset_requested.clear()
                        scores = self._process_block(
                            raw_block,
                            pipeline=pipeline,
                            sample_id=source.sample_id,
                            sample_index=source_index,
                            sample_count=len(sources),
                            detect_samples=analysis_enabled,
                        )
                        for offset in np.flatnonzero(
                            scores <= self._detector_config.end_threshold
                        ):
                            if len(background_lines) >= self.gap_lines:
                                break
                            background_lines.append(
                                np.asarray(raw_block[int(offset)]).copy()
                            )
                        if not self._pace(stop - start):
                            return
                    del raw

                    if background_lines:
                        gap_raw = np.stack(
                            [
                                background_lines[index % len(background_lines)]
                                for index in range(self.gap_lines)
                            ]
                        )
                    else:
                        dark_raw = np.rint(calibrator.dark).astype(source.dtype)
                        gap_raw = np.repeat(
                            dark_raw[None, :, :],
                            self.gap_lines,
                            axis=0,
                        )
                    for start in range(0, self.gap_lines, self.chunk_lines):
                        if not self._wait_until_running():
                            return
                        stop = min(start + self.chunk_lines, self.gap_lines)
                        analysis_enabled = self._analysis_enabled.is_set()
                        reset_requested = self._analysis_reset_requested.is_set()
                        if analysis_enabled != analysis_was_enabled or reset_requested:
                            pipeline.reset()
                            analysis_was_enabled = analysis_enabled
                            self._analysis_reset_requested.clear()
                        self._process_block(
                            gap_raw[start:stop],
                            pipeline=pipeline,
                            sample_id=source.sample_id,
                            sample_index=source_index,
                            sample_count=len(sources),
                            detect_samples=analysis_enabled,
                        )
                        if not self._pace(stop - start):
                            return

                self._messages.put(
                    StreamMessage(
                        kind="cycle_complete",
                        sample_count=len(sources),
                    )
                )
                if not self.loop:
                    return
        except Exception:
            self._messages.put(
                StreamMessage(
                    kind="error",
                    error=traceback.format_exc(),
                )
            )
