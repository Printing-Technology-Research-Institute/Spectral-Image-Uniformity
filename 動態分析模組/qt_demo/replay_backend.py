from __future__ import annotations

import json
import re
from dataclasses import dataclass
from pathlib import Path

import numpy as np


ENVI_DTYPE = {
    1: np.uint8,
    2: np.int16,
    3: np.int32,
    4: np.float32,
    5: np.float64,
    12: np.uint16,
    13: np.uint32,
    14: np.int64,
    15: np.uint64,
}


def _header_value(text: str, key: str, default: int | None = None) -> int:
    match = re.search(rf"(?im)^\s*{re.escape(key)}\s*=\s*([^\r\n]+)", text)
    if match:
        return int(match.group(1).strip())
    if default is not None:
        return default
    raise ValueError(f"ENVI header is missing {key!r}")


def _wavelengths(text: str) -> np.ndarray | None:
    match = re.search(r"(?is)wavelength\s*=\s*\{(?P<body>.*?)\}", text)
    if not match:
        return None
    values = [
        float(item.group(0))
        for item in re.finditer(r"[-+]?\d+(?:\.\d+)?", match.group("body"))
    ]
    return np.array(values, dtype=np.float32)


@dataclass(frozen=True)
class ReplaySample:
    sample_id: str
    hdr_path: Path
    raw_path: Path
    hdr_lines: int
    actual_lines: int
    line_mismatch: bool
    detected_start_line: int | None
    detected_end_line: int | None
    cae_status: str
    uniformity: float | None
    heatmap_path: Path | None
    resistance_prediction: float
    resistance_loocv: float
    resistance_measured: float


def load_manifest(path: Path) -> tuple[dict, list[ReplaySample]]:
    document = json.loads(path.read_text(encoding="utf-8"))
    samples = []
    for row in document["samples"]:
        samples.append(
            ReplaySample(
                sample_id=row["sample_id"],
                hdr_path=Path(row["hdr_path"]),
                raw_path=Path(row["raw_path"]),
                hdr_lines=int(row["hdr_lines"]),
                actual_lines=int(row["actual_lines"]),
                line_mismatch=bool(row["line_mismatch"]),
                detected_start_line=(
                    int(row["detected_start_line"])
                    if row.get("detected_start_line") is not None
                    else None
                ),
                detected_end_line=(
                    int(row["detected_end_line"])
                    if row.get("detected_end_line") is not None
                    else None
                ),
                cae_status=row["cae_status"],
                uniformity=(
                    float(row["uniformity"])
                    if row["uniformity"] is not None
                    else None
                ),
                heatmap_path=(
                    Path(row["heatmap_path"]) if row["heatmap_path"] else None
                ),
                resistance_prediction=float(row["resistance_prediction"]),
                resistance_loocv=float(row["resistance_loocv"]),
                resistance_measured=float(row["resistance_measured"]),
            )
        )
    return document, samples


class EnviLineReplay:
    def __init__(self, sample: ReplaySample, target_wavelength_nm: float = 1350.0) -> None:
        text = sample.hdr_path.read_text(encoding="utf-8", errors="ignore")
        width = _header_value(text, "samples")
        bands = _header_value(text, "bands")
        header_offset = _header_value(text, "header offset", 0)
        dtype_code = _header_value(text, "data type")
        byte_order = _header_value(text, "byte order", 0)
        if dtype_code not in ENVI_DTYPE:
            raise ValueError(f"Unsupported ENVI data type {dtype_code}")
        dtype = np.dtype(ENVI_DTYPE[dtype_code]).newbyteorder(
            "<" if byte_order == 0 else ">"
        )
        wavelengths = _wavelengths(text)
        if wavelengths is not None and len(wavelengths) == bands:
            band_index = int(np.argmin(np.abs(wavelengths - target_wavelength_nm)))
        else:
            band_index = bands // 2

        raw = np.memmap(
            sample.raw_path,
            dtype=dtype,
            mode="r",
            offset=header_offset,
            shape=(sample.actual_lines, bands, width),
        )
        image = np.asarray(raw[:, band_index, :], dtype=np.float32)
        image = np.nan_to_num(image, nan=0.0, posinf=0.0, neginf=0.0)
        lo, hi = np.percentile(image, [1.0, 99.0])
        if hi <= lo:
            hi = lo + 1.0
        normalized = np.clip((image - lo) / (hi - lo), 0.0, 1.0)
        self.image = np.round(normalized * 255.0).astype(np.uint8)
        self.lines = self.image.shape[0]

    def display_lines(self, start: int, stop: int) -> np.ndarray:
        start = max(0, min(int(start), self.lines))
        stop = max(start, min(int(stop), self.lines))
        return self.image[start:stop]

    def frame(self, cursor: int, viewport_lines: int = 280) -> np.ndarray:
        viewport_lines = min(viewport_lines, self.lines)
        start = min(max(cursor - viewport_lines, 0), self.lines - viewport_lines)
        gray = self.image[start : start + viewport_lines]
        return np.repeat(gray[:, :, None], 3, axis=2)


class RollingLineBuffer:
    """Display buffer shared by recorded replay and future GenTL line input."""

    def __init__(self, width: int, viewport_lines: int = 280) -> None:
        if width < 1 or viewport_lines < 1:
            raise ValueError("Rolling line buffer dimensions must be positive")
        self.width = int(width)
        self.viewport_lines = int(viewport_lines)
        self._image = np.zeros(
            (self.viewport_lines, self.width, 3),
            dtype=np.uint8,
        )

    def clear(self) -> None:
        self._image.fill(0)

    def push(self, lines: np.ndarray) -> None:
        incoming = np.asarray(lines)
        if incoming.ndim == 2:
            incoming = np.repeat(incoming[:, :, None], 3, axis=2)
        if incoming.ndim != 3 or incoming.shape[1:] != (self.width, 3):
            raise ValueError(
                f"Expected display lines shaped (n, {self.width}) or "
                f"(n, {self.width}, 3), received {incoming.shape}."
            )
        incoming = np.ascontiguousarray(incoming, dtype=np.uint8)
        line_count = incoming.shape[0]
        if line_count == 0:
            return
        if line_count >= self.viewport_lines:
            self._image[:] = incoming[-self.viewport_lines :]
            return
        self._image[:-line_count] = self._image[line_count:]
        self._image[-line_count:] = incoming

    def frame(self) -> np.ndarray:
        return self._image.copy()
