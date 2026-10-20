from __future__ import annotations

from collections.abc import Sequence

import numpy as np

from sample_boundary_detector import (
    DetectorConfig,
    LineCalibrator,
    SampleBoundaryDetector,
)
from stream_source import StreamMessage
from stream_visualization import (
    DEFAULT_FALSE_COLOR_WAVELENGTHS_NM,
    detector_image_with_range,
    false_color_lines,
    resolve_band_indices,
)


class RawLinePipeline:
    """Shared calibration, visualization, and boundary pipeline for raw sources."""

    def __init__(
        self,
        *,
        calibrator: LineCalibrator,
        detector_config: DetectorConfig,
        wavelengths: np.ndarray | None = None,
        false_color_wavelengths_nm: Sequence[float] = (
            DEFAULT_FALSE_COLOR_WAVELENGTHS_NM
        ),
        preview_min: float = 0.0,
        preview_max: float = 0.9,
        preview_gamma: float = 1.0,
        detector_percentile_low: float = 1.0,
        detector_percentile_high: float = 99.0,
    ) -> None:
        if preview_max <= preview_min:
            raise ValueError("preview_max must be greater than preview_min")
        if preview_gamma <= 0:
            raise ValueError("preview_gamma must be positive")
        if not 0.0 <= detector_percentile_low < detector_percentile_high <= 100.0:
            raise ValueError("Invalid detector contrast percentiles")
        targets = tuple(float(value) for value in false_color_wavelengths_nm)
        if len(targets) != 3:
            raise ValueError("False-color preview requires three wavelengths")

        self.calibrator = calibrator
        self.detector = SampleBoundaryDetector(
            detector_config,
            collect_cube=True,
            collect_raw_cube=True,
        )
        self.false_color_wavelengths_nm = targets
        self.preview_min = float(preview_min)
        self.preview_max = float(preview_max)
        self.preview_gamma = float(preview_gamma)
        self.detector_percentile_low = float(detector_percentile_low)
        self.detector_percentile_high = float(detector_percentile_high)
        self.last_scores = np.empty(0, dtype=np.float32)
        self.set_wavelengths(wavelengths)

    @property
    def raw_shape(self) -> tuple[int, int]:
        return tuple(int(value) for value in self.calibrator.dark.shape)

    def set_wavelengths(self, wavelengths: np.ndarray | None) -> None:
        available = (
            None
            if wavelengths is None
            else np.asarray(wavelengths, dtype=np.float32)
        )
        self.detector_spectral_range_nm = (
            (float(available.min()), float(available.max()))
            if (
                available is not None
                and available.ndim == 1
                and available.size == self.raw_shape[0]
            )
            else None
        )
        self.band_indices, self.actual_wavelengths_nm = resolve_band_indices(
            available,
            self.raw_shape[0],
            self.false_color_wavelengths_nm,
        )

    def reset(self) -> None:
        """Discard any partial boundary/cube state before a new analysis run."""
        self.detector.reset()
        self.last_scores = np.empty(0, dtype=np.float32)

    def process_block(
        self,
        raw_block: np.ndarray,
        *,
        sample_id: str,
        sample_index: int | None = None,
        sample_count: int | None = None,
        detect_samples: bool = True,
        frame_ids: Sequence[int | None] | None = None,
        timestamps_ns: Sequence[int | None] | None = None,
    ) -> list[StreamMessage]:
        block = np.asarray(raw_block)
        if block.ndim == 2:
            block = block[None, ...]
        if block.ndim != 3 or block.shape[1:] != self.raw_shape:
            raise ValueError(
                f"Expected raw block shape (lines, {self.raw_shape[0]}, "
                f"{self.raw_shape[1]}), received {block.shape}."
            )
        if block.shape[0] < 1:
            return []
        if frame_ids is not None and len(frame_ids) != block.shape[0]:
            raise ValueError("frame_ids must match the raw block line count")
        if timestamps_ns is not None and len(timestamps_ns) != block.shape[0]:
            raise ValueError("timestamps_ns must match the raw block line count")

        corrected = self.calibrator.calibrate_block(block)
        scores = self.calibrator.line_scores(corrected)
        self.last_scores = scores
        preview = false_color_lines(
            corrected,
            self.band_indices,
            reflectance_min=self.preview_min,
            reflectance_max=self.preview_max,
            gamma=self.preview_gamma,
        )
        spectral_frame, detector_range = detector_image_with_range(
            block[-1],
            percentile_low=self.detector_percentile_low,
            percentile_high=self.detector_percentile_high,
        )
        messages = [
            StreamMessage(
                kind="preview",
                sample_id=sample_id,
                sample_index=sample_index,
                sample_count=sample_count,
                display_lines=preview,
                detector_frame=spectral_frame,
                detector_value_range=detector_range,
                detector_spectral_range_nm=self.detector_spectral_range_nm,
                false_color_wavelengths_nm=self.actual_wavelengths_nm,
                material_score=float(scores[-1]),
            )
        ]
        if detect_samples:
            for offset, score in enumerate(scores):
                payload = self.calibrator.model_line(corrected[offset])
                raw_payload = np.ascontiguousarray(block[offset].T)
                for event in self.detector.feed(
                    float(score),
                    payload,
                    raw_payload,
                    frame_id=(None if frame_ids is None else frame_ids[offset]),
                    timestamp_ns=(
                        None if timestamps_ns is None else timestamps_ns[offset]
                    ),
                ):
                    messages.append(
                        StreamMessage(
                            kind=event.kind,
                            sample_id=sample_id,
                            sample_index=sample_index,
                            sample_count=sample_count,
                            cube=event.cube,
                            raw_cube=event.raw_cube,
                            material_score=event.score,
                            sample_line_count=event.sample_line_count,
                            acquisition_integrity=event.acquisition_integrity,
                        )
                    )
                    event.cube = None
                    event.raw_cube = None
        return messages

    def flush(
        self,
        *,
        sample_id: str,
        sample_index: int | None = None,
        sample_count: int | None = None,
    ) -> list[StreamMessage]:
        messages: list[StreamMessage] = []
        for event in self.detector.flush():
            messages.append(
                StreamMessage(
                    kind=event.kind,
                    sample_id=sample_id,
                    sample_index=sample_index,
                    sample_count=sample_count,
                    cube=event.cube,
                    raw_cube=event.raw_cube,
                    material_score=event.score,
                    sample_line_count=event.sample_line_count,
                    acquisition_integrity=event.acquisition_integrity,
                )
            )
            event.cube = None
            event.raw_cube = None
        return messages
