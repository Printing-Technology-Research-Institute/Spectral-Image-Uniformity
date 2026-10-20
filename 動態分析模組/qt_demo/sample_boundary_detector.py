from __future__ import annotations

from dataclasses import asdict, dataclass, field
from pathlib import Path
from typing import Literal

import numpy as np


@dataclass(frozen=True)
class DetectorConfig:
    material_pixel_threshold: float = 0.08
    start_threshold: float = 0.20
    end_threshold: float = 0.04
    start_confirm_lines: int = 4
    end_confirm_lines: int = 8
    min_sample_lines: int = 200
    max_sample_lines: int = 1200

    def validate(self) -> None:
        if not 0.0 <= self.end_threshold < self.start_threshold <= 1.0:
            raise ValueError("Expected 0 <= end_threshold < start_threshold <= 1")
        if self.start_confirm_lines < 1 or self.end_confirm_lines < 1:
            raise ValueError("Confirmation line counts must be positive")
        if self.min_sample_lines < 1 or self.max_sample_lines < self.min_sample_lines:
            raise ValueError("Invalid minimum/maximum sample line counts")


@dataclass
class BoundaryEvent:
    kind: Literal["sample_start", "sample_end", "sample_rejected"]
    stream_line: int
    sample_start_line: int | None
    sample_end_line: int | None
    sample_line_count: int
    score: float
    reason: str
    acquisition_integrity: dict | None = None
    cube: np.ndarray | None = field(default=None, repr=False)
    raw_cube: np.ndarray | None = field(default=None, repr=False)


class LineCalibrator:
    def __init__(
        self,
        dark: np.ndarray,
        white: np.ndarray,
        *,
        trim_bands: int = 10,
        clip_max: float = 1.2,
        min_white_column_fraction: float = 0.2,
        material_pixel_threshold: float = 0.08,
        spatial_roi_start: int = 0,
        spatial_roi_end: int | None = None,
    ) -> None:
        dark = np.asarray(dark, dtype=np.float32)
        white = np.asarray(white, dtype=np.float32)
        if dark.ndim != 2 or dark.shape != white.shape:
            raise ValueError("Dark and white references must share a (bands, width) shape")
        if trim_bands < 0 or trim_bands * 2 >= dark.shape[0]:
            raise ValueError("trim_bands removes every spectral band")

        width = int(dark.shape[1])
        roi_start = int(spatial_roi_start)
        roi_end = width if spatial_roi_end is None else int(spatial_roi_end)
        if not 0 <= roi_start < roi_end <= width:
            raise ValueError(
                "Expected 0 <= spatial_roi_start < spatial_roi_end <= width"
            )

        denominator = white - dark
        positive_response = denominator > 1e-6
        if float(positive_response.mean()) < 0.5:
            raise ValueError(
                "White reference is not brighter than dark reference over "
                "at least half of the detector pixels"
            )
        usable_response = np.where(positive_response, denominator, 0.0)
        column_response = usable_response.mean(axis=0)
        column_threshold = float(np.median(column_response) * min_white_column_fraction)
        reference_valid_columns = (
            (column_response >= column_threshold)
            & (positive_response.mean(axis=0) >= 0.5)
        )
        roi_columns = np.zeros(width, dtype=bool)
        roi_columns[roi_start:roi_end] = True
        valid_columns = reference_valid_columns & roi_columns
        valid_indices = np.flatnonzero(valid_columns)
        minimum_valid_columns = max(1, (roi_end - roi_start + 1) // 2)
        if valid_indices.size < minimum_valid_columns:
            raise ValueError("Fewer than half of the spatial ROI columns are valid")

        self.dark = dark
        self.calibration_valid_pixels = (
            positive_response & reference_valid_columns[None, :]
        )
        self.valid_pixels = positive_response & valid_columns[None, :]
        self.denominator = np.where(
            self.calibration_valid_pixels,
            denominator,
            1.0,
        ).astype(np.float32)
        self.valid_columns = valid_columns
        self.valid_column_first = int(valid_indices[0])
        self.valid_column_last = int(valid_indices[-1])
        self.spatial_roi_start = roi_start
        self.spatial_roi_end = roi_end
        self.trim_bands = int(trim_bands)
        self.clip_max = float(clip_max)
        self.material_pixel_threshold = float(material_pixel_threshold)
        self.column_response_threshold = column_threshold

    @classmethod
    def from_files(
        cls,
        dark_path: str | Path,
        white_path: str | Path,
        *,
        bands: int,
        width: int,
        **kwargs,
    ) -> "LineCalibrator":
        shape = (bands, width)
        dark = np.fromfile(Path(dark_path), dtype="<f4")
        white = np.fromfile(Path(white_path), dtype="<f4")
        expected = bands * width
        if dark.size != expected or white.size != expected:
            raise ValueError(f"Reference files must contain {expected} float32 values")
        return cls(dark.reshape(shape), white.reshape(shape), **kwargs)

    @property
    def output_bands(self) -> int:
        return self.dark.shape[0] - 2 * self.trim_bands

    def calibrate_block(self, raw_block: np.ndarray) -> np.ndarray:
        block = np.asarray(raw_block, dtype=np.float32)
        if block.ndim == 2:
            block = block[None, ...]
        if block.ndim != 3 or block.shape[1:] != self.dark.shape:
            raise ValueError(
                f"Expected raw block shape (lines, {self.dark.shape[0]}, {self.dark.shape[1]})"
            )
        corrected = (block - self.dark[None, :, :]) / (
            self.denominator[None, :, :] + 1e-6
        )
        corrected = np.nan_to_num(corrected, nan=0.0, posinf=0.0, neginf=0.0)
        np.clip(corrected, 0.0, self.clip_max, out=corrected)
        # Keep valid calibrated pixels outside the spatial ROI for the detector
        # preview and saved diagnostic cube. The ROI is applied only to sample
        # scoring here and explicitly to the model mask in live_analysis.py.
        corrected[:, ~self.calibration_valid_pixels] = 0.0
        return corrected.astype(np.float32, copy=False)

    def line_scores(self, corrected_block: np.ndarray) -> np.ndarray:
        block = np.asarray(corrected_block, dtype=np.float32)
        if block.ndim == 2:
            block = block[None, ...]
        stop = block.shape[1] - self.trim_bands
        selected = block[:, self.trim_bands : stop, :]
        spatial_signal = selected.mean(axis=1)
        material = (
            spatial_signal[:, self.valid_columns] > self.material_pixel_threshold
        )
        return material.mean(axis=1, dtype=np.float64).astype(np.float32)

    def model_line(self, corrected_line: np.ndarray) -> np.ndarray:
        stop = corrected_line.shape[0] - self.trim_bands
        return np.ascontiguousarray(
            corrected_line[self.trim_bands : stop, :].T,
            dtype=np.float32,
        )

    def metadata(self) -> dict:
        return {
            "bands": int(self.dark.shape[0]),
            "width": int(self.dark.shape[1]),
            "output_bands": self.output_bands,
            "trim_bands": self.trim_bands,
            "clip_max": self.clip_max,
            "material_pixel_threshold": self.material_pixel_threshold,
            "spatial_roi_start": self.spatial_roi_start,
            "spatial_roi_end": self.spatial_roi_end,
            "spatial_roi_width": self.spatial_roi_end - self.spatial_roi_start,
            "valid_column_count": int(self.valid_columns.sum()),
            "valid_column_first": self.valid_column_first,
            "valid_column_last": self.valid_column_last,
            "valid_pixel_count": int(self.valid_pixels.sum()),
            "invalid_pixel_count": int(self.valid_pixels.size - self.valid_pixels.sum()),
            "calibrated_pixel_count": int(self.calibration_valid_pixels.sum()),
            "white_column_response_threshold": self.column_response_threshold,
        }


class SampleBoundaryDetector:
    def __init__(
        self,
        config: DetectorConfig | None = None,
        *,
        collect_cube: bool = False,
        collect_raw_cube: bool = False,
    ) -> None:
        self.config = config or DetectorConfig()
        self.config.validate()
        self.collect_cube = bool(collect_cube)
        self.collect_raw_cube = bool(collect_raw_cube)
        self.reset()

    def reset(self) -> None:
        self.state: Literal["idle", "active"] = "idle"
        self.next_stream_line = 0
        self.sample_start_line: int | None = None
        self.last_sample_line: int | None = None
        self.sample_line_count = 0
        self._pending_start: list[
            tuple[int, np.ndarray | None, np.ndarray | None, int | None, int | None]
        ] = []
        self._pending_end: list[
            tuple[int, np.ndarray | None, np.ndarray | None, int | None, int | None]
        ] = []
        self._frame_ids: list[int | None] = []
        self._timestamps_ns: list[int | None] = []
        self._cube_buffer: np.ndarray | None = None
        self._raw_cube_buffer: np.ndarray | None = None

    def _copy_payload(self, payload: np.ndarray | None) -> np.ndarray | None:
        if not self.collect_cube:
            return None
        if payload is None:
            raise ValueError("collect_cube=True requires a payload for every line")
        return np.asarray(payload, dtype=np.float32).copy()

    def _copy_raw_payload(
        self,
        raw_payload: np.ndarray | None,
    ) -> np.ndarray | None:
        if not self.collect_raw_cube:
            return None
        if raw_payload is None:
            raise ValueError(
                "collect_raw_cube=True requires a raw payload for every line"
            )
        return np.ascontiguousarray(raw_payload).copy()

    def _append_payload(
        self,
        stream_line: int,
        payload: np.ndarray | None,
        raw_payload: np.ndarray | None,
        frame_id: int | None,
        timestamp_ns: int | None,
    ) -> None:
        if self.collect_cube:
            if payload is None:
                raise ValueError("collect_cube=True requires a payload for every line")
            if self._cube_buffer is None:
                self._cube_buffer = np.empty(
                    (self.config.max_sample_lines, *payload.shape),
                    dtype=np.float32,
                )
            self._cube_buffer[self.sample_line_count] = payload
        if self.collect_raw_cube:
            if raw_payload is None:
                raise ValueError(
                    "collect_raw_cube=True requires a raw payload for every line"
                )
            if self._raw_cube_buffer is None:
                self._raw_cube_buffer = np.empty(
                    (self.config.max_sample_lines, *raw_payload.shape),
                    dtype=raw_payload.dtype,
                )
            self._raw_cube_buffer[self.sample_line_count] = raw_payload
        self.sample_line_count += 1
        self.last_sample_line = stream_line
        self._frame_ids.append(None if frame_id is None else int(frame_id))
        self._timestamps_ns.append(
            None if timestamp_ns is None else int(timestamp_ns)
        )

    def _acquisition_integrity(self) -> dict:
        known_ids = [value for value in self._frame_ids if value is not None]
        gaps = [
            current - previous
            for previous, current in zip(known_ids, known_ids[1:])
            if current > previous
        ]
        missing = sum(max(0, gap - 1) for gap in gaps)
        known_timestamps = [
            value for value in self._timestamps_ns if value is not None
        ]
        timestamp_deltas_ms = [
            (current - previous) / 1_000_000.0
            for previous, current in zip(known_timestamps, known_timestamps[1:])
            if current > previous
        ]
        ids_complete = len(known_ids) == self.sample_line_count
        status = (
            "incomplete"
            if missing > 0
            else ("complete" if ids_complete else "unavailable")
        )
        return {
            "status": status,
            "received_lines": self.sample_line_count,
            "frame_id_known_count": len(known_ids),
            "frame_id_first": known_ids[0] if known_ids else None,
            "frame_id_last": known_ids[-1] if known_ids else None,
            "frame_id_missing_count": int(missing),
            "largest_frame_gap": max(gaps, default=None),
            "timestamp_known_count": len(known_timestamps),
            "timestamp_interval_median_ms": (
                float(np.median(timestamp_deltas_ms))
                if timestamp_deltas_ms
                else None
            ),
            "timestamp_interval_max_ms": (
                float(max(timestamp_deltas_ms))
                if timestamp_deltas_ms
                else None
            ),
        }

    def _finish(self, stream_line: int, score: float, reason: str) -> BoundaryEvent:
        start = self.sample_start_line
        end = self.last_sample_line
        count = self.sample_line_count
        accepted = count >= self.config.min_sample_lines
        cube = None
        raw_cube = None
        if accepted and self.collect_cube and self._cube_buffer is not None:
            cube = self._cube_buffer[:count]
        if (
            accepted
            and self.collect_raw_cube
            and self._raw_cube_buffer is not None
        ):
            raw_cube = self._raw_cube_buffer[:count]
        event = BoundaryEvent(
            kind="sample_end" if accepted else "sample_rejected",
            stream_line=stream_line,
            sample_start_line=start,
            sample_end_line=end,
            sample_line_count=count,
            score=float(score),
            reason=reason if accepted else "shorter_than_min_sample_lines",
            acquisition_integrity=self._acquisition_integrity(),
            cube=cube,
            raw_cube=raw_cube,
        )
        self.state = "idle"
        self.sample_start_line = None
        self.last_sample_line = None
        self.sample_line_count = 0
        self._pending_start = []
        self._pending_end = []
        self._cube_buffer = None
        self._raw_cube_buffer = None
        self._frame_ids = []
        self._timestamps_ns = []
        return event

    def feed(
        self,
        score: float,
        payload: np.ndarray | None = None,
        raw_payload: np.ndarray | None = None,
        *,
        stream_line: int | None = None,
        frame_id: int | None = None,
        timestamp_ns: int | None = None,
    ) -> list[BoundaryEvent]:
        if stream_line is None:
            stream_line = self.next_stream_line
        if stream_line < self.next_stream_line:
            raise ValueError("stream_line must be monotonic")
        self.next_stream_line = stream_line + 1
        score = float(score)
        copied = self._copy_payload(payload)
        copied_raw = self._copy_raw_payload(raw_payload)
        events: list[BoundaryEvent] = []

        if self.state == "idle":
            if score >= self.config.start_threshold:
                self._pending_start.append(
                    (stream_line, copied, copied_raw, frame_id, timestamp_ns)
                )
                if len(self._pending_start) >= self.config.start_confirm_lines:
                    self.state = "active"
                    self.sample_start_line = self._pending_start[0][0]
                    for (
                        index,
                        pending_payload,
                        pending_raw,
                        pending_frame_id,
                        pending_timestamp_ns,
                    ) in self._pending_start:
                        self._append_payload(
                            index,
                            pending_payload,
                            pending_raw,
                            pending_frame_id,
                            pending_timestamp_ns,
                        )
                    self._pending_start = []
                    events.append(
                        BoundaryEvent(
                            kind="sample_start",
                            stream_line=stream_line,
                            sample_start_line=self.sample_start_line,
                            sample_end_line=None,
                            sample_line_count=self.sample_line_count,
                            score=score,
                            reason="start_confirmed",
                        )
                    )
            else:
                self._pending_start = []
            return events

        if score <= self.config.end_threshold:
            self._pending_end.append(
                (stream_line, copied, copied_raw, frame_id, timestamp_ns)
            )
            if len(self._pending_end) >= self.config.end_confirm_lines:
                events.append(self._finish(stream_line, score, "end_confirmed"))
            return events

        for (
            index,
            pending_payload,
            pending_raw,
            pending_frame_id,
            pending_timestamp_ns,
        ) in self._pending_end:
            self._append_payload(
                index,
                pending_payload,
                pending_raw,
                pending_frame_id,
                pending_timestamp_ns,
            )
        self._pending_end = []
        self._append_payload(
            stream_line,
            copied,
            copied_raw,
            frame_id,
            timestamp_ns,
        )
        if self.sample_line_count >= self.config.max_sample_lines:
            events.append(self._finish(stream_line, score, "max_sample_lines"))
        return events

    def flush(self) -> list[BoundaryEvent]:
        if self.state != "active":
            self._pending_start = []
            return []
        stream_line = max(self.next_stream_line - 1, 0)
        return [self._finish(stream_line, 0.0, "stream_flushed")]

    def config_dict(self) -> dict:
        return asdict(self.config)
