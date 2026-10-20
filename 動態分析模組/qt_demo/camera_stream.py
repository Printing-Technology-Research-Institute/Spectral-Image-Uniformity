from __future__ import annotations

import json
import os
import queue
import threading
import time
import traceback
from dataclasses import dataclass, field
from datetime import datetime, timezone
from logging import getLogger
from pathlib import Path
from typing import Any, Literal

import numpy as np
from genicam import genapi
from genicam.gentl import TimeoutException
from harvesters.core import Harvester

from acquisition_diagnostics import AcquisitionDiagnosticsLogger
from raw_line_pipeline import RawLinePipeline
from replay_backend import _wavelengths
from sample_boundary_detector import DetectorConfig, LineCalibrator
from stream_source import StreamMessage
from stream_visualization import (
    DEFAULT_FALSE_COLOR_WAVELENGTHS_NM,
    detector_image_with_range,
)


APP_DIR = Path(__file__).resolve().parent
WORKSPACE = APP_DIR.parents[1]
DEFAULT_CTI_PATH = APP_DIR.parent / "fx_test" / "mvGenTLProducer.cti"
DEFAULT_MVIMPACT_DATA_DIR = WORKSPACE / "vendor" / "mvimpact_2_47_data"
DEFAULT_DARK_REFERENCE = (
    WORKSPACE / "20260528_導電材料" / "50u_1-Ref-Dark.dat"
)
DEFAULT_WHITE_REFERENCE = (
    WORKSPACE / "20260528_導電材料" / "50u_1-Ref-White.dat"
)
DEFAULT_WAVELENGTH_HEADER = (
    WORKSPACE / "20260528_導電材料" / "50u_1.hdr"
)


def _configure_mvimpact_runtime(
    cti_path: Path,
    *,
    data_dir: Path = DEFAULT_MVIMPACT_DATA_DIR,
) -> Path | None:
    """Configure a portable Impact Acquire runtime before loading its CTI."""
    resolved_cti = Path(cti_path).resolve()
    if (
        resolved_cti.parent.name.casefold() != "x64"
        or resolved_cti.parent.parent.name.casefold() != "bin"
    ):
        return None

    runtime_dir = resolved_cti.parents[2]
    if not resolved_cti.is_file():
        return None

    resolved_data_dir = Path(data_dir).resolve()
    if not resolved_data_dir.is_dir():
        resolved_data_dir.mkdir(parents=True, exist_ok=True)
    os.environ["MVIMPACT_ACQUIRE_DIR"] = str(runtime_dir)
    os.environ["MVIMPACT_ACQUIRE_DATA_DIR"] = str(resolved_data_dir)

    current_path = os.environ.get("PATH", "")
    current_entries = {
        entry.casefold()
        for entry in current_path.split(os.pathsep)
        if entry
    }
    prepend = []
    for directory in (resolved_cti.parent, runtime_dir):
        text = str(directory)
        if text.casefold() not in current_entries:
            prepend.append(text)
            current_entries.add(text.casefold())
    if prepend:
        os.environ["PATH"] = os.pathsep.join([*prepend, current_path])
    return runtime_dir


@dataclass(frozen=True)
class _CalibrationRequest:
    kind: Literal["dark", "white"]
    path: Path
    frame_count: int
    settle_frames: int
    settle_seconds: float
    activate_after_capture: bool = True


@dataclass
class _CalibrationCapture:
    request: _CalibrationRequest
    restore_shutter_open: bool
    ready_at: float
    remaining_settle_frames: int
    frames: list[np.ndarray] = field(default_factory=list)


@dataclass(frozen=True)
class _CalibrationFilesRequest:
    dark_path: Path
    white_path: Path


def _create_acquirer(harvester: Harvester, device_index: int) -> Any:
    create = getattr(harvester, "create", None)
    if create is not None:
        return create(device_index)
    return harvester.create_image_acquirer(list_index=device_index)


def _start_acquisition(image_acquirer: Any) -> None:
    start = getattr(image_acquirer, "start", None)
    if start is not None:
        start()
    else:
        image_acquirer.start_acquisition()


def _stop_acquisition(image_acquirer: Any) -> None:
    stop = getattr(image_acquirer, "stop", None)
    if stop is not None:
        stop()
    else:
        image_acquirer.stop_acquisition()


def _fetch_buffer(image_acquirer: Any, timeout: float) -> Any:
    fetch = getattr(image_acquirer, "fetch", None)
    if fetch is not None:
        return fetch(timeout=timeout)
    return image_acquirer.fetch_buffer(timeout=timeout)


def _configure_buffer_pool(
    image_acquirer: Any,
    requested_buffers: int,
) -> int | None:
    """Increase the Harvester input pool when the backend exposes num_buffers."""
    requested = int(requested_buffers)
    if requested < 1:
        raise ValueError("requested_buffers must be positive")
    try:
        current = getattr(image_acquirer, "num_buffers", None)
        target = requested if current is None else max(int(current), requested)
        image_acquirer.num_buffers = target
        applied = getattr(image_acquirer, "num_buffers", target)
        return int(applied)
    except Exception:
        # Some Harvester/GenTL combinations keep buffer management internal.
        # Acquisition remains usable; frame IDs and the diagnostics log will
        # reveal whether that backend is still dropping frames.
        return None


def _buffer_frame_id(buffer: Any) -> int | None:
    try:
        return int(buffer.module.frame_id)
    except Exception:
        return None


def _buffer_timestamp_ns(buffer: Any) -> int | None:
    try:
        return int(buffer.timestamp_ns)
    except Exception:
        return None


@dataclass(frozen=True)
class _LinePacket:
    raw_line: np.ndarray
    frame_id: int | None
    timestamp_ns: int | None
    generation: int


class _LineProcessingWorker:
    """Calibrate/detect lines outside the time-critical camera fetch loop."""

    def __init__(
        self,
        *,
        pipeline: RawLinePipeline,
        messages: queue.Queue[StreamMessage],
        chunk_lines: int,
        queue_capacity_lines: int,
        pipeline_config: dict[str, Any],
        diagnostics: AcquisitionDiagnosticsLogger,
    ) -> None:
        if chunk_lines < 1 or queue_capacity_lines < chunk_lines:
            raise ValueError(
                "processing queue must hold at least one complete chunk"
            )
        self.chunk_lines = int(chunk_lines)
        self._messages = messages
        self._pipeline_config = pipeline_config
        self._diagnostics = diagnostics
        self._queue: queue.Queue[_LinePacket | None] = queue.Queue(
            maxsize=int(queue_capacity_lines)
        )
        self._state_lock = threading.Lock()
        self._pipeline = pipeline
        self._analysis_enabled = False
        self._generation = 0
        self._queue_drops = 0
        self._accepting = True
        self._force_stop = threading.Event()
        self._thread = threading.Thread(
            target=self._run,
            name="fx17-line-processing",
            daemon=True,
        )
        self._thread.start()

    @property
    def queue_depth(self) -> int:
        return self._queue.qsize()

    @property
    def queue_drops(self) -> int:
        with self._state_lock:
            return int(self._queue_drops)

    def set_state(
        self,
        *,
        pipeline: RawLinePipeline | None = None,
        analysis_enabled: bool | None = None,
        reset: bool = False,
    ) -> None:
        with self._state_lock:
            changed = bool(reset)
            if pipeline is not None and pipeline is not self._pipeline:
                self._pipeline = pipeline
                changed = True
            if (
                analysis_enabled is not None
                and bool(analysis_enabled) != self._analysis_enabled
            ):
                self._analysis_enabled = bool(analysis_enabled)
                changed = True
            if changed:
                self._generation += 1

    def submit_line(
        self,
        raw_line: np.ndarray,
        *,
        frame_id: int | None,
        timestamp_ns: int | None,
    ) -> bool:
        with self._state_lock:
            if not self._accepting:
                return False
            generation = self._generation
        packet = _LinePacket(
            raw_line=raw_line,
            frame_id=frame_id,
            timestamp_ns=timestamp_ns,
            generation=generation,
        )
        try:
            self._queue.put_nowait(packet)
            return True
        except queue.Full:
            with self._state_lock:
                self._queue_drops += 1
            return False

    def close(self, *, timeout: float = 3.0, drain: bool = True) -> None:
        deadline = time.monotonic() + max(0.1, float(timeout))
        with self._state_lock:
            self._accepting = False
        if not drain:
            self._discard_queue()
            self._force_stop.set()
        while True:
            try:
                self._queue.put(None, timeout=0.05)
                break
            except queue.Full:
                if time.monotonic() >= deadline:
                    self._force_stop.set()
                    self._discard_queue()
        self._thread.join(timeout=max(0.0, deadline - time.monotonic()))
        if self._thread.is_alive():
            self._force_stop.set()
            self._discard_queue()
            try:
                self._queue.put_nowait(None)
            except queue.Full:
                pass
            self._thread.join(timeout=0.5)

    def _discard_queue(self) -> None:
        while True:
            try:
                self._queue.get_nowait()
            except queue.Empty:
                return

    def _state(self) -> tuple[int, RawLinePipeline, bool]:
        with self._state_lock:
            return self._generation, self._pipeline, self._analysis_enabled

    def _emit_block(
        self,
        pipeline: RawLinePipeline,
        lines: list[np.ndarray],
        frame_ids: list[int | None],
        timestamps_ns: list[int | None],
        *,
        sample_number: int,
        generation: int,
    ) -> int:
        sample_id = f"FX17_{sample_number:04d}"
        started = time.perf_counter()
        messages = pipeline.process_block(
            np.stack(lines),
            sample_id=sample_id,
            frame_ids=frame_ids,
            timestamps_ns=timestamps_ns,
        )
        processing_ms = (time.perf_counter() - started) * 1000.0
        self._diagnostics.record_block(
            block_lines=len(lines),
            processing_ms=processing_ms,
            processing_queue_depth=self.queue_depth,
            processing_queue_drops=self.queue_drops,
        )
        current_generation, _, _ = self._state()
        if generation != current_generation:
            return sample_number
        for message in messages:
            message.sample_id = sample_id
            self._messages.put(message)
            if message.kind in ("sample_end", "sample_rejected"):
                sample_number += 1
                sample_id = f"FX17_{sample_number:04d}"
        return sample_number

    def _run(self) -> None:
        active_generation = -1
        active_pipeline: RawLinePipeline | None = None
        active_analysis = False
        pending_lines: list[np.ndarray] = []
        pending_frame_ids: list[int | None] = []
        pending_timestamps_ns: list[int | None] = []
        detector_preview_counter = 0
        sample_number = 1
        try:
            while True:
                packet = self._queue.get()
                if packet is None:
                    break
                if self._force_stop.is_set():
                    break
                generation, pipeline, analysis_enabled = self._state()
                if packet.generation != generation:
                    continue
                if generation != active_generation:
                    pipeline.reset()
                    active_generation = generation
                    active_pipeline = pipeline
                    active_analysis = analysis_enabled
                    pending_lines = []
                    pending_frame_ids = []
                    pending_timestamps_ns = []
                    detector_preview_counter = 0
                assert active_pipeline is not None
                if not active_analysis:
                    detector_preview_counter += 1
                    if detector_preview_counter >= self.chunk_lines:
                        detector_preview_counter = 0
                        corrected_preview = (
                            active_pipeline.calibrator.calibrate_block(
                                packet.raw_line
                            )
                        )
                        material_score = float(
                            active_pipeline.calibrator.line_scores(
                                corrected_preview
                            )[0]
                        )
                        detector_frame, detector_range = (
                            detector_image_with_range(
                                packet.raw_line,
                                percentile_low=self._pipeline_config[
                                    "detector_percentile_low"
                                ],
                                percentile_high=self._pipeline_config[
                                    "detector_percentile_high"
                                ],
                            )
                        )
                        self._messages.put(
                            StreamMessage(
                                kind="preview",
                                detector_frame=detector_frame,
                                detector_value_range=detector_range,
                                detector_spectral_range_nm=(
                                    active_pipeline.detector_spectral_range_nm
                                ),
                                material_score=material_score,
                            )
                        )
                    continue

                pending_lines.append(packet.raw_line)
                pending_frame_ids.append(packet.frame_id)
                pending_timestamps_ns.append(packet.timestamp_ns)
                if len(pending_lines) >= self.chunk_lines:
                    sample_number = self._emit_block(
                        active_pipeline,
                        pending_lines,
                        pending_frame_ids,
                        pending_timestamps_ns,
                        sample_number=sample_number,
                        generation=active_generation,
                    )
                    pending_lines = []
                    pending_frame_ids = []
                    pending_timestamps_ns = []

            if (
                not self._force_stop.is_set()
                and active_pipeline is not None
                and active_analysis
            ):
                if pending_lines:
                    sample_number = self._emit_block(
                        active_pipeline,
                        pending_lines,
                        pending_frame_ids,
                        pending_timestamps_ns,
                        sample_number=sample_number,
                        generation=active_generation,
                    )
                for message in active_pipeline.flush(
                    sample_id=f"FX17_{sample_number:04d}"
                ):
                    self._messages.put(message)
        except Exception:
            self._messages.put(
                StreamMessage(kind="error", error=traceback.format_exc())
            )


class FX17CameraStreamWorker:
    """Harvester camera source for the shared Qt stream pipeline."""

    def __init__(
        self,
        *,
        cti_path: str | Path = DEFAULT_CTI_PATH,
        dark_ref_path: str | Path = DEFAULT_DARK_REFERENCE,
        white_ref_path: str | Path = DEFAULT_WHITE_REFERENCE,
        wavelength_hdr_path: str | Path = DEFAULT_WAVELENGTH_HEADER,
        device_index: int = 0,
        serial_number: str | None = None,
        bands: int = 224,
        width: int = 640,
        chunk_lines: int = 20,
        processing_queue_lines: int = 2048,
        driver_buffer_count: int = 128,
        acquisition_log_enabled: bool = True,
        acquisition_log_dir: str | Path | None = WORKSPACE / "acquisition_logs",
        fetch_timeout: float = 0.5,
        max_consecutive_timeouts: int = 20,
        discovery_attempts: int = 12,
        discovery_retry_interval: float = 1.0,
        calibration_frame_count: int = 64,
        calibration_settle_frames: int = 8,
        calibration_settle_seconds: float = 0.5,
        configure_camera: bool = True,
        exposure_us: float = 1200.0,
        line_rate_hz: float | None = None,
        open_shutter_on_start: bool = True,
        close_shutter_on_pause: bool = True,
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
    ) -> None:
        if device_index < 0:
            raise ValueError("device_index must not be negative")
        if bands < 3 or width < 1 or chunk_lines < 1:
            raise ValueError("Invalid FX17 buffer dimensions or chunk size")
        if processing_queue_lines < chunk_lines:
            raise ValueError(
                "processing_queue_lines must hold at least one chunk"
            )
        if driver_buffer_count < 1:
            raise ValueError("driver_buffer_count must be positive")
        if fetch_timeout <= 0 or max_consecutive_timeouts < 1:
            raise ValueError("Invalid camera timeout settings")
        if discovery_attempts < 1 or discovery_retry_interval < 0:
            raise ValueError("Invalid camera discovery retry settings")
        if (
            calibration_frame_count < 1
            or calibration_settle_frames < 0
            or calibration_settle_seconds < 0
        ):
            raise ValueError("Invalid camera calibration capture settings")
        if exposure_us <= 0:
            raise ValueError("exposure_us must be positive")
        if line_rate_hz is not None and (
            not np.isfinite(float(line_rate_hz)) or float(line_rate_hz) <= 0
        ):
            raise ValueError("line_rate_hz must be a positive finite number")

        self.cti_path = Path(cti_path).resolve()
        self.dark_ref_path = Path(dark_ref_path).resolve()
        self.white_ref_path = Path(white_ref_path).resolve()
        self.wavelength_hdr_path = Path(wavelength_hdr_path).resolve()
        self.device_index = int(device_index)
        self.configured_serial_number = (
            str(serial_number).strip() if serial_number is not None else None
        )
        self.serial_number = self.configured_serial_number
        self.bands = int(bands)
        self.width = int(width)
        self.chunk_lines = int(chunk_lines)
        self.processing_queue_lines = int(processing_queue_lines)
        self.driver_buffer_count = int(driver_buffer_count)
        self.acquisition_log_enabled = bool(acquisition_log_enabled)
        self.acquisition_log_dir = (
            None
            if acquisition_log_dir is None
            else Path(acquisition_log_dir).expanduser().resolve()
        )
        self.fetch_timeout = float(fetch_timeout)
        self.max_consecutive_timeouts = int(max_consecutive_timeouts)
        self.discovery_attempts = int(discovery_attempts)
        self.discovery_retry_interval = float(discovery_retry_interval)
        self.calibration_frame_count = int(calibration_frame_count)
        self.calibration_settle_frames = int(calibration_settle_frames)
        self.calibration_settle_seconds = float(calibration_settle_seconds)
        self.configure_camera = bool(configure_camera)
        self.exposure_us = float(exposure_us)
        self.initial_line_rate_hz = (
            None if line_rate_hz is None else float(line_rate_hz)
        )
        self.open_shutter_on_start = bool(open_shutter_on_start)
        self.close_shutter_on_pause = bool(close_shutter_on_pause)
        self._pipeline_config = {
            "false_color_wavelengths_nm": tuple(
                float(value) for value in false_color_wavelengths_nm
            ),
            "preview_min": float(preview_min),
            "preview_max": float(preview_max),
            "preview_gamma": float(preview_gamma),
            "detector_percentile_low": float(detector_percentile_low),
            "detector_percentile_high": float(detector_percentile_high),
        }
        self._calibration_config = {
            "trim_bands": int(trim_bands),
            "clip_max": float(clip_max),
            "min_white_column_fraction": float(min_white_column_fraction),
            "material_pixel_threshold": float(material_pixel_threshold),
            "spatial_roi_start": int(spatial_roi_start),
            "spatial_roi_end": (
                None if spatial_roi_end is None else int(spatial_roi_end)
            ),
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
        self._commands: queue.Queue[tuple[str, object | None]] = queue.Queue()
        self._active_calibration: _CalibrationCapture | None = None
        self._pending_pipeline_replacement: RawLinePipeline | None = None
        self._run_event = threading.Event()
        self._run_event.set()
        self._analysis_enabled = threading.Event()
        self._analysis_reset_requested = threading.Event()
        self._stop_event = threading.Event()
        self._thread = threading.Thread(
            target=self._run,
            name="fx17-camera-stream",
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
        """Reset partial boundary/cube state without interrupting acquisition."""
        self._analysis_reset_requested.set()

    def request_line_rate(self) -> None:
        self._commands.put(("request_line_rate", None))

    def set_line_rate_hz(self, line_rate_hz: float) -> None:
        value = float(line_rate_hz)
        if not np.isfinite(value) or value <= 0:
            raise ValueError("line_rate_hz must be a positive finite number")
        self._commands.put(("set_line_rate_hz", value))

    def set_exposure_us(self, exposure_us: float) -> None:
        value = float(exposure_us)
        if value <= 0:
            raise ValueError("exposure_us must be positive")
        self._commands.put(("set_exposure_us", value))

    def open_shutter(self) -> None:
        self._commands.put(("shutter_on", None))

    def close_shutter(self) -> None:
        self._commands.put(("shutter_off", None))

    def capture_reference(
        self,
        kind: Literal["dark", "white"],
        path: str | Path,
        *,
        frame_count: int | None = None,
        activate_after_capture: bool = True,
    ) -> None:
        if kind not in ("dark", "white"):
            raise ValueError("Calibration kind must be 'dark' or 'white'")
        count = (
            self.calibration_frame_count
            if frame_count is None
            else int(frame_count)
        )
        if count < 1:
            raise ValueError("Calibration frame_count must be positive")
        output_path = Path(path).expanduser()
        if not output_path.suffix:
            output_path = output_path.with_suffix(".dat")
        request = _CalibrationRequest(
            kind=kind,
            path=output_path.resolve(),
            frame_count=count,
            settle_frames=self.calibration_settle_frames,
            settle_seconds=self.calibration_settle_seconds,
            activate_after_capture=bool(activate_after_capture),
        )
        self._commands.put(("capture_reference", request))

    def set_calibration_files(
        self,
        dark_ref_path: str | Path,
        white_ref_path: str | Path,
    ) -> None:
        dark_path = Path(dark_ref_path).expanduser().resolve()
        white_path = Path(white_ref_path).expanduser().resolve()
        self._commands.put(
            (
                "set_calibration_files",
                _CalibrationFilesRequest(
                    dark_path=dark_path,
                    white_path=white_path,
                ),
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

    def _build_pipeline(self) -> RawLinePipeline:
        for path in (
            self.cti_path,
            self.dark_ref_path,
            self.white_ref_path,
            self.wavelength_hdr_path,
        ):
            if not path.is_file():
                raise FileNotFoundError(path)
        calibrator = LineCalibrator.from_files(
            self.dark_ref_path,
            self.white_ref_path,
            bands=self.bands,
            width=self.width,
            **self._calibration_config,
        )
        header = self.wavelength_hdr_path.read_text(
            encoding="utf-8",
            errors="ignore",
        )
        wavelengths = _wavelengths(header)
        return RawLinePipeline(
            calibrator=calibrator,
            detector_config=self._detector_config,
            wavelengths=wavelengths,
            **self._pipeline_config,
        )

    def _write_reference_files(
        self,
        request: _CalibrationRequest,
        reference: np.ndarray,
    ) -> None:
        output = request.path
        output.parent.mkdir(parents=True, exist_ok=True)
        stored = np.ascontiguousarray(reference, dtype="<f4")
        if stored.shape != (self.bands, self.width):
            raise ValueError(
                f"Expected calibration shape {(self.bands, self.width)}, "
                f"received {stored.shape}."
            )

        temporary = output.with_name(f".{output.name}.tmp")
        metadata_path = output.with_suffix(output.suffix + ".json")
        metadata_temporary = metadata_path.with_name(
            f".{metadata_path.name}.tmp"
        )
        metadata = {
            "schema_version": 1,
            "calibration_kind": request.kind,
            "data_file": output.name,
            "dtype": "little-endian float32",
            "shape": [self.bands, self.width],
            "frame_count": request.frame_count,
            "settle_frames": request.settle_frames,
            "settle_seconds": request.settle_seconds,
            "activate_after_capture": request.activate_after_capture,
            "exposure_us": self.exposure_us,
            "exposure_ms": self.exposure_us / 1000.0,
            "camera_serial_number": self.serial_number,
            "created_utc": datetime.now(timezone.utc).isoformat(),
            "minimum": float(stored.min()),
            "median": float(np.median(stored)),
            "maximum": float(stored.max()),
        }
        try:
            with temporary.open("wb") as stream:
                stored.tofile(stream)
            metadata_temporary.write_text(
                json.dumps(metadata, ensure_ascii=False, indent=2),
                encoding="utf-8",
            )
            os.replace(temporary, output)
            os.replace(metadata_temporary, metadata_path)
        finally:
            temporary.unlink(missing_ok=True)
            metadata_temporary.unlink(missing_ok=True)

    @staticmethod
    def _gen_feature(feature_root: Any) -> dict[str, Any]:
        return {
            item.node.name: (
                {
                    child.node.name: child
                    for child in item.features
                }
                if (
                    item.node.principal_interface_type
                    == genapi.EInterfaceType.intfICategory
                )
                else item
            )
            for item in feature_root.features
        }

    def _init_features(self, image_acquirer: Any) -> dict[str, Any]:
        node_map = image_acquirer.remote_device.node_map
        return {
            "Specim": self._gen_feature(node_map.Specim),
            "AcquisitionControl": self._gen_feature(
                node_map.AcquisitionControl
            ),
            "ImageFormatControl": self._gen_feature(
                node_map.ImageFormatControl
            ),
        }

    @staticmethod
    def _set_shutter(features: dict[str, Any], *, opened: bool) -> None:
        motor = features["Specim"]["MotorShutter"]
        if opened:
            motor["MotorShutter_PulseRev"].value = 190
            motor["MotorShutter_PulseRev"].value = 200
        else:
            motor["MotorShutter_PulseFwd"].value = 190
            motor["MotorShutter_PulseFwd"].value = 200

    @staticmethod
    def _set_exposure(features: dict[str, Any], exposure_us: float) -> None:
        features["AcquisitionControl"]["ExposureTime"].value = float(
            exposure_us
        )

    def _configure_features(self, features: dict[str, Any]) -> None:
        features["ImageFormatControl"]["PixelFormat"].value = "Mono12Packed"
        self._set_exposure(features, self.exposure_us)
        initial_line_rate_hz = getattr(self, "initial_line_rate_hz", None)
        if initial_line_rate_hz is None:
            features["AcquisitionControl"][
                "EnAcquisitionFrameRate"
            ].value = False
        else:
            self._write_line_rate(features, initial_line_rate_hz)

    @staticmethod
    def _line_rate_nodes(features: dict[str, Any]) -> tuple[Any, Any, Any, Any]:
        acquisition = features["AcquisitionControl"]
        enable = acquisition.get("EnAcquisitionFrameRate")
        if enable is None:
            enable = acquisition.get("AcquisitionFrameRateEnable")
        rate = acquisition.get("AcquisitionFrameRate")
        minimum = acquisition.get("AcquisitionFrameRateMin")
        maximum = acquisition.get("AcquisitionFrameRateMax")
        if enable is None or rate is None or minimum is None or maximum is None:
            raise RuntimeError(
                "FX17 AcquisitionControl does not expose the required "
                "frame-rate nodes."
            )
        return enable, rate, minimum, maximum

    @classmethod
    def _read_line_rate(cls, features: dict[str, Any]) -> dict[str, float | bool]:
        enable, rate, minimum, maximum = cls._line_rate_nodes(features)
        enabled = bool(enable.value)
        minimum_hz = float(minimum.value)
        maximum_hz = float(maximum.value)
        # FX17 makes AcquisitionFrameRate unreadable while control is disabled;
        # in that free-running mode the device operates at its reported maximum.
        current_hz = float(rate.value) if enabled else maximum_hz
        return {
            "line_rate_hz": current_hz,
            "line_rate_min_hz": minimum_hz,
            "line_rate_max_hz": maximum_hz,
            "line_rate_control_enabled": enabled,
        }

    @classmethod
    def _write_line_rate(
        cls,
        features: dict[str, Any],
        requested_hz: float,
    ) -> dict[str, float | bool]:
        enable, rate, minimum, maximum = cls._line_rate_nodes(features)
        previous_enabled = bool(enable.value)
        previous_rate_hz = float(rate.value) if previous_enabled else None
        minimum_hz = float(minimum.value)
        maximum_hz = float(maximum.value)
        if not minimum_hz <= requested_hz <= maximum_hz:
            raise ValueError(
                f"Requested line rate {requested_hz:.3f} Hz is outside the "
                f"FX17 range {minimum_hz:.3f}–{maximum_hz:.3f} Hz."
            )
        try:
            if not previous_enabled:
                enable.value = True
            rate.value = float(requested_hz)
            applied_hz = float(rate.value)
            tolerance = max(0.01, abs(requested_hz) * 0.001)
            if abs(applied_hz - requested_hz) > tolerance:
                raise RuntimeError(
                    f"FX17 read-back mismatch: requested {requested_hz:.3f} Hz, "
                    f"read {applied_hz:.3f} Hz."
                )
        except Exception:
            try:
                if previous_enabled and previous_rate_hz is not None:
                    rate.value = previous_rate_hz
                else:
                    enable.value = False
            except Exception:
                pass
            raise
        return {
            "line_rate_hz": applied_hz,
            "line_rate_min_hz": float(minimum.value),
            "line_rate_max_hz": float(maximum.value),
            "line_rate_control_enabled": bool(enable.value),
        }

    def _apply_commands(
        self,
        features: dict[str, Any],
        *,
        shutter_is_open: bool,
    ) -> bool:
        while True:
            try:
                command, value = self._commands.get_nowait()
            except queue.Empty:
                return shutter_is_open
            if command == "set_exposure_us":
                assert value is not None
                self.exposure_us = float(value)
                self._set_exposure(features, self.exposure_us)
            elif command == "shutter_on":
                self._set_shutter(features, opened=True)
                shutter_is_open = True
            elif command == "shutter_off":
                self._set_shutter(features, opened=False)
                shutter_is_open = False
            elif command == "capture_reference":
                if not isinstance(value, _CalibrationRequest):
                    raise TypeError("Invalid calibration capture request")
                if self._active_calibration is not None:
                    self._messages.put(
                        StreamMessage(
                            kind="calibration_failed",
                            calibration_kind=value.kind,
                            calibration_path=str(value.path),
                            error="Another calibration capture is already running.",
                        )
                    )
                    continue
                target_shutter_open = value.kind == "white"
                restore_shutter_open = shutter_is_open
                if shutter_is_open != target_shutter_open:
                    self._set_shutter(
                        features,
                        opened=target_shutter_open,
                    )
                    shutter_is_open = target_shutter_open
                self._active_calibration = _CalibrationCapture(
                    request=value,
                    restore_shutter_open=restore_shutter_open,
                    ready_at=time.monotonic() + value.settle_seconds,
                    remaining_settle_frames=value.settle_frames,
                )
                self._messages.put(
                    StreamMessage(
                        kind="calibration_started",
                        calibration_kind=value.kind,
                        calibration_path=str(value.path),
                        calibration_frame_count=value.frame_count,
                    )
                )
            elif command == "set_calibration_files":
                if not isinstance(value, _CalibrationFilesRequest):
                    raise TypeError("Invalid calibration file request")
                previous_dark = self.dark_ref_path
                previous_white = self.white_ref_path
                self.dark_ref_path = value.dark_path
                self.white_ref_path = value.white_path
                try:
                    replacement_pipeline = self._build_pipeline()
                except Exception:
                    self.dark_ref_path = previous_dark
                    self.white_ref_path = previous_white
                    self._messages.put(
                        StreamMessage(
                            kind="calibration_files_failed",
                            dark_ref_path=str(value.dark_path),
                            white_ref_path=str(value.white_path),
                            error=traceback.format_exc(),
                        )
                    )
                else:
                    self._pending_pipeline_replacement = replacement_pipeline
                    self._messages.put(
                        StreamMessage(
                            kind="calibration_files_loaded",
                            dark_ref_path=str(value.dark_path),
                            white_ref_path=str(value.white_path),
                            calibration_activated=True,
                        )
                    )
            elif command == "request_line_rate":
                try:
                    status = self._read_line_rate(features)
                except Exception:
                    self._messages.put(
                        StreamMessage(
                            kind="line_rate_failed",
                            error=traceback.format_exc(),
                        )
                    )
                else:
                    self._messages.put(
                        StreamMessage(
                            kind="line_rate_report",
                            **status,
                        )
                    )
            elif command == "set_line_rate_hz":
                assert value is not None
                requested_hz = float(value)
                try:
                    status = self._write_line_rate(
                        features,
                        requested_hz,
                    )
                except Exception:
                    self._messages.put(
                        StreamMessage(
                            kind="line_rate_failed",
                            line_rate_requested_hz=requested_hz,
                            error=traceback.format_exc(),
                        )
                    )
                else:
                    self._messages.put(
                        StreamMessage(
                            kind="line_rate_set",
                            line_rate_requested_hz=requested_hz,
                            **status,
                        )
                    )
            else:
                raise ValueError(f"Unknown camera command: {command}")

    def _take_pipeline_replacement(
        self,
        pipeline: RawLinePipeline,
    ) -> RawLinePipeline:
        replacement = self._pending_pipeline_replacement
        if replacement is None:
            return pipeline
        self._pending_pipeline_replacement = None
        return replacement

    def _consume_calibration_line(
        self,
        raw_line: np.ndarray,
        pipeline: RawLinePipeline,
        features: dict[str, Any],
        *,
        shutter_is_open: bool,
    ) -> tuple[RawLinePipeline, bool, bool]:
        capture = self._active_calibration
        if capture is None:
            return pipeline, shutter_is_open, False
        request = capture.request
        if time.monotonic() < capture.ready_at:
            return pipeline, shutter_is_open, True
        if capture.remaining_settle_frames:
            capture.remaining_settle_frames -= 1
            return pipeline, shutter_is_open, True

        capture.frames.append(
            np.ascontiguousarray(raw_line, dtype=np.float32)
        )
        if len(capture.frames) < request.frame_count:
            return pipeline, shutter_is_open, True

        reference = np.mean(
            np.stack(capture.frames),
            axis=0,
            dtype=np.float64,
        ).astype(np.float32)
        path_attribute = (
            "dark_ref_path" if request.kind == "dark" else "white_ref_path"
        )
        try:
            self._write_reference_files(request, reference)
        except Exception:
            self._messages.put(
                StreamMessage(
                    kind="calibration_failed",
                    calibration_kind=request.kind,
                    calibration_path=str(request.path),
                    calibration_frame_count=request.frame_count,
                    error=traceback.format_exc(),
                )
            )
        else:
            setattr(self, path_attribute, request.path)
            if not request.activate_after_capture:
                self._messages.put(
                    StreamMessage(
                        kind="calibration_complete",
                        calibration_kind=request.kind,
                        calibration_path=str(request.path),
                        calibration_frame_count=request.frame_count,
                        calibration_activated=False,
                    )
                )
            else:
                try:
                    replacement_pipeline = self._build_pipeline()
                except Exception:
                    self._messages.put(
                        StreamMessage(
                            kind="calibration_complete",
                            calibration_kind=request.kind,
                            calibration_path=str(request.path),
                            calibration_frame_count=request.frame_count,
                            calibration_activated=False,
                            error=traceback.format_exc(),
                        )
                    )
                else:
                    pipeline = replacement_pipeline
                    self._messages.put(
                        StreamMessage(
                            kind="calibration_complete",
                            calibration_kind=request.kind,
                            calibration_path=str(request.path),
                            calibration_frame_count=request.frame_count,
                            calibration_activated=True,
                        )
                    )
        finally:
            if shutter_is_open != capture.restore_shutter_open:
                self._set_shutter(
                    features,
                    opened=capture.restore_shutter_open,
                )
                shutter_is_open = capture.restore_shutter_open
            self._active_calibration = None
        return pipeline, shutter_is_open, True

    @staticmethod
    def _device_field(device: Any, name: str) -> str:
        value = getattr(device, name, "")
        return "" if value is None else str(value).strip()

    @classmethod
    def _is_fx17_device(cls, device: Any) -> bool:
        identity = " ".join(
            cls._device_field(device, name)
            for name in ("model", "id_", "display_name")
        ).casefold()
        return "fx17" in identity

    @classmethod
    def _device_identity(cls, device: Any) -> tuple[str, str, str]:
        model = cls._device_field(device, "model") or "FX17"
        serial = cls._device_field(device, "serial_number")
        vendor = cls._device_field(device, "vendor")
        return model, serial, vendor

    def _select_device_index(self, devices: list[Any]) -> int:
        fx17_devices = [
            (index, device)
            for index, device in enumerate(devices)
            if self._is_fx17_device(device)
        ]
        if self.configured_serial_number:
            for index, device in fx17_devices:
                if (
                    self._device_field(device, "serial_number")
                    == self.configured_serial_number
                ):
                    return index
        if len(fx17_devices) == 1:
            return fx17_devices[0][0]
        if not fx17_devices:
            raise RuntimeError(
                "No FX17 camera was found in the GenTL device list; "
                f"available devices: {devices}"
            )
        available = [
            self._device_identity(device)[:2]
            for _, device in fx17_devices
        ]
        configured = (
            f"Configured serial {self.configured_serial_number!r} was not found. "
            if self.configured_serial_number
            else ""
        )
        raise RuntimeError(
            f"{configured}Multiple FX17 cameras were discovered; refusing to "
            f"select one automatically: {available}"
        )

    def _discover_devices(self, harvester: Harvester) -> list[Any]:
        """Retry transient GigE Vision discovery failures without touching nodes."""
        for attempt in range(1, self.discovery_attempts + 1):
            harvester.update()
            devices = list(harvester.device_info_list)
            if devices or self._stop_event.is_set():
                return devices
            if attempt < self.discovery_attempts:
                self._stop_event.wait(self.discovery_retry_interval)
        return []

    def _copy_component(self, buffer: Any) -> tuple[np.ndarray, str]:
        components = buffer.payload.components
        if not components:
            raise RuntimeError("FX17 buffer has no payload components.")
        component = components[0]
        data_format = str(component.data_format)
        components_per_pixel = int(component.num_components_per_pixel)
        # Copy before queueing/releasing the Harvester buffer.
        copied = np.array(component.data, copy=True)
        expected_elements = self.bands * self.width
        if components_per_pixel != 1:
            raise ValueError(
                f"Expected one component per pixel, received {components_per_pixel}."
            )
        if int(component.height) != self.bands or int(component.width) != self.width:
            raise ValueError(
                f"Expected FX17 component {self.bands}x{self.width}, received "
                f"{component.height}x{component.width} ({data_format})."
            )
        if copied.size != expected_elements:
            raise ValueError(
                f"Expected {expected_elements} decoded elements, received "
                f"{copied.size} ({copied.dtype}, {data_format}). "
                "Packed data was not decoded to one value per pixel."
            )
        if not np.issubdtype(copied.dtype, np.integer):
            raise TypeError(
                f"Expected an integer FX17 component, received {copied.dtype}."
            )
        return (
            np.ascontiguousarray(copied.reshape(self.bands, self.width)),
            data_format,
        )

    def _run(self) -> None:
        _configure_mvimpact_runtime(self.cti_path)
        harvester = Harvester(logger=getLogger())
        image_acquirer = None
        processor: _LineProcessingWorker | None = None
        diagnostics = AcquisitionDiagnosticsLogger(None, enabled=False)
        acquisition_started = False
        features = None
        shutter_is_open = False
        analysis_was_enabled = False
        calibration_was_active = False
        try:
            pipeline = self._build_pipeline()
            harvester.add_file(str(self.cti_path))
            devices = self._discover_devices(harvester)
            if not devices:
                if self._stop_event.is_set():
                    return
                raise RuntimeError(
                    "No GenTL camera was discovered after "
                    f"{self.discovery_attempts} attempts. IP ping reachability "
                    "does not guarantee GigE Vision discovery/control access. "
                    "Close test.py and vendor camera tools, then power-cycle the "
                    "FX17 if it was terminated without cleanup. The camera worker "
                    "did not write exposure, pixel format, frame rate, or shutter "
                    "nodes."
                )
            selected_index = self._select_device_index(devices)
            selected_device = devices[selected_index]
            selected_model, selected_serial, selected_vendor = (
                self._device_identity(selected_device)
            )
            auto_selected = (
                not self.configured_serial_number
                or selected_serial != self.configured_serial_number
            )
            if auto_selected:
                # A saved line rate belongs to the previously configured camera.
                # Keep a newly discovered unit in its own free-running mode until
                # the user performs pushbroom acquisition calibration.
                self.initial_line_rate_hz = None
            if selected_serial:
                self.serial_number = selected_serial
            image_acquirer = _create_acquirer(harvester, selected_index)
            features = self._init_features(image_acquirer)
            if self.configure_camera:
                self._configure_features(features)
            applied_buffer_count = _configure_buffer_pool(
                image_acquirer,
                self.driver_buffer_count,
            )
            try:
                current_line_rate_hz = float(
                    self._read_line_rate(features)["line_rate_hz"]
                )
            except Exception:
                current_line_rate_hz = self.initial_line_rate_hz
            try:
                diagnostics = AcquisitionDiagnosticsLogger(
                    self.acquisition_log_dir,
                    enabled=self.acquisition_log_enabled,
                    expected_line_rate_hz=current_line_rate_hz,
                    session_label=selected_serial or self.serial_number,
                    session_metadata={
                        "camera_model": selected_model,
                        "camera_serial_number": (
                            selected_serial or self.serial_number
                        ),
                        "bands": self.bands,
                        "width": self.width,
                        "chunk_lines": self.chunk_lines,
                        "processing_queue_lines": self.processing_queue_lines,
                        "driver_buffer_count_requested": (
                            self.driver_buffer_count
                        ),
                        "driver_buffer_count_applied": applied_buffer_count,
                        "exposure_us": self.exposure_us,
                    },
                )
            except Exception:
                getLogger(__name__).exception(
                    "FX17 acquisition diagnostics could not be started"
                )
                diagnostics = AcquisitionDiagnosticsLogger(None, enabled=False)
            processor = _LineProcessingWorker(
                pipeline=pipeline,
                messages=self._messages,
                chunk_lines=self.chunk_lines,
                queue_capacity_lines=self.processing_queue_lines,
                pipeline_config=self._pipeline_config,
                diagnostics=diagnostics,
            )
            analysis_was_enabled = self._analysis_enabled.is_set()
            processor.set_state(
                analysis_enabled=analysis_was_enabled,
                reset=True,
            )
            self._messages.put(
                StreamMessage(
                    kind="camera_connected",
                    camera_model=selected_model,
                    camera_serial_number=selected_serial or None,
                    camera_vendor=selected_vendor or None,
                    camera_auto_selected=auto_selected,
                )
            )
            getLogger(__name__).info(
                "FX17 acquisition started: driver_buffers=%s, "
                "processing_queue_lines=%s, diagnostics=%s",
                applied_buffer_count,
                self.processing_queue_lines,
                (
                    diagnostics.paths.csv_path
                    if diagnostics.paths is not None
                    else "disabled"
                ),
            )
            consecutive_timeouts = 0
            while not self._stop_event.is_set():
                if not self._run_event.is_set() and acquisition_started:
                    if self.close_shutter_on_pause and shutter_is_open:
                        self._set_shutter(features, opened=False)
                        shutter_is_open = False
                    _stop_acquisition(image_acquirer)
                    acquisition_started = False
                if not self._wait_until_running():
                    break
                if not acquisition_started:
                    shutter_is_open = self._apply_commands(
                        features,
                        shutter_is_open=shutter_is_open,
                    )
                    replacement_pipeline = self._take_pipeline_replacement(
                        pipeline
                    )
                    if replacement_pipeline is not pipeline:
                        pipeline = replacement_pipeline
                        processor.set_state(pipeline=pipeline, reset=True)
                    _start_acquisition(image_acquirer)
                    acquisition_started = True
                    dark_capture_active = (
                        self._active_calibration is not None
                        and self._active_calibration.request.kind == "dark"
                    )
                    if (
                        self.open_shutter_on_start
                        and not shutter_is_open
                        and not dark_capture_active
                    ):
                        self._set_shutter(features, opened=True)
                        shutter_is_open = True

                if not self._run_event.is_set():
                    continue
                shutter_is_open = self._apply_commands(
                    features,
                    shutter_is_open=shutter_is_open,
                )
                replacement_pipeline = self._take_pipeline_replacement(pipeline)
                if replacement_pipeline is not pipeline:
                    pipeline = replacement_pipeline
                    processor.set_state(pipeline=pipeline, reset=True)
                analysis_enabled = self._analysis_enabled.is_set()
                reset_requested = self._analysis_reset_requested.is_set()
                if analysis_enabled != analysis_was_enabled or reset_requested:
                    processor.set_state(
                        analysis_enabled=analysis_enabled,
                        reset=True,
                    )
                    analysis_was_enabled = analysis_enabled
                    self._analysis_reset_requested.clear()
                calibration_active = self._active_calibration is not None
                if calibration_active != calibration_was_active:
                    processor.set_state(reset=True)
                    calibration_was_active = calibration_active
                fetch_started = time.perf_counter()
                try:
                    buffer = _fetch_buffer(image_acquirer, self.fetch_timeout)
                except TimeoutException:
                    buffer = None
                fetch_wait_ms = (time.perf_counter() - fetch_started) * 1000.0
                if buffer is None:
                    consecutive_timeouts += 1
                    diagnostics.record_timeout(
                        consecutive_timeouts=consecutive_timeouts
                    )
                    if consecutive_timeouts >= self.max_consecutive_timeouts:
                        raise TimeoutError(
                            f"FX17 timed out {consecutive_timeouts} consecutive times."
                        )
                    continue

                consecutive_timeouts = 0
                with buffer:
                    copy_started = time.perf_counter()
                    raw_line, _ = self._copy_component(buffer)
                    frame_id = _buffer_frame_id(buffer)
                    timestamp_ns = _buffer_timestamp_ns(buffer)
                    copy_ms = (time.perf_counter() - copy_started) * 1000.0
                pipeline, shutter_is_open, calibration_consumed = (
                    self._consume_calibration_line(
                        raw_line,
                        pipeline,
                        features,
                        shutter_is_open=shutter_is_open,
                    )
                )
                if pipeline is not replacement_pipeline:
                    processor.set_state(pipeline=pipeline, reset=True)
                if calibration_was_active and self._active_calibration is None:
                    processor.set_state(
                        pipeline=pipeline,
                        analysis_enabled=analysis_enabled,
                        reset=True,
                    )
                    calibration_was_active = False
                if calibration_consumed:
                    diagnostics.record_frame(
                        frame_id=frame_id,
                        camera_timestamp_ns=timestamp_ns,
                        fetch_wait_ms=fetch_wait_ms,
                        copy_ms=copy_ms,
                        processing_queue_depth=processor.queue_depth,
                        processing_queue_drops=processor.queue_drops,
                    )
                    continue
                processor.submit_line(
                    raw_line,
                    frame_id=frame_id,
                    timestamp_ns=timestamp_ns,
                )
                diagnostics.record_frame(
                    frame_id=frame_id,
                    camera_timestamp_ns=timestamp_ns,
                    fetch_wait_ms=fetch_wait_ms,
                    copy_ms=copy_ms,
                    processing_queue_depth=processor.queue_depth,
                    processing_queue_drops=processor.queue_drops,
                )
        except Exception:
            self._messages.put(
                StreamMessage(
                    kind="error",
                    error=traceback.format_exc(),
                )
            )
        finally:
            if image_acquirer is not None:
                if (
                    features is not None
                    and shutter_is_open
                ):
                    try:
                        self._set_shutter(features, opened=False)
                    except Exception:
                        pass
                if acquisition_started:
                    try:
                        _stop_acquisition(image_acquirer)
                    except Exception:
                        pass
                try:
                    image_acquirer.destroy()
                except Exception:
                    pass
            if processor is not None:
                processor.close(timeout=3.0, drain=True)
            diagnostics.close(timeout=3.0)
            harvester.reset()
