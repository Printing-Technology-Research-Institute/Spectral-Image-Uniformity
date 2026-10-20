from __future__ import annotations

from dataclasses import dataclass, field
from typing import Literal, Protocol

import numpy as np


@dataclass
class StreamMessage:
    kind: Literal[
        "preview",
        "sample_start",
        "sample_end",
        "sample_rejected",
        "cycle_complete",
        "calibration_started",
        "calibration_complete",
        "calibration_failed",
        "calibration_files_loaded",
        "calibration_files_failed",
        "camera_connected",
        "line_rate_report",
        "line_rate_set",
        "line_rate_failed",
        "error",
    ]
    sample_id: str | None = None
    sample_index: int | None = None
    sample_count: int | None = None
    display_lines: np.ndarray | None = field(default=None, repr=False)
    detector_frame: np.ndarray | None = field(default=None, repr=False)
    detector_value_range: tuple[float, float] | None = None
    detector_spectral_range_nm: tuple[float, float] | None = None
    false_color_wavelengths_nm: tuple[float, float, float] | None = None
    cube: np.ndarray | None = field(default=None, repr=False)
    raw_cube: np.ndarray | None = field(default=None, repr=False)
    material_score: float | None = None
    sample_line_count: int | None = None
    acquisition_integrity: dict | None = None
    calibration_kind: Literal["dark", "white"] | None = None
    calibration_path: str | None = None
    dark_ref_path: str | None = None
    white_ref_path: str | None = None
    calibration_frame_count: int | None = None
    calibration_activated: bool | None = None
    camera_model: str | None = None
    camera_serial_number: str | None = None
    camera_vendor: str | None = None
    camera_auto_selected: bool | None = None
    line_rate_hz: float | None = None
    line_rate_requested_hz: float | None = None
    line_rate_min_hz: float | None = None
    line_rate_max_hz: float | None = None
    line_rate_control_enabled: bool | None = None
    error: str | None = None


class StreamWorker(Protocol):
    def resume(self) -> None: ...

    def pause(self) -> None: ...

    def set_analysis_enabled(self, enabled: bool) -> None: ...

    def reset_analysis(self) -> None: ...

    def request_line_rate(self) -> None: ...

    def set_line_rate_hz(self, line_rate_hz: float) -> None: ...

    def poll(self) -> list[StreamMessage]: ...

    def close(self, timeout: float = 2.0) -> None: ...
