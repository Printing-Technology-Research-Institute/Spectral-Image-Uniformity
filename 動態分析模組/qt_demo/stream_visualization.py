from __future__ import annotations

from collections.abc import Sequence

import numpy as np


DEFAULT_FALSE_COLOR_WAVELENGTHS_NM = (1650.0, 1300.0, 1000.0)


def resolve_band_indices(
    wavelengths: np.ndarray | None,
    band_count: int,
    targets_nm: Sequence[float] = DEFAULT_FALSE_COLOR_WAVELENGTHS_NM,
) -> tuple[tuple[int, int, int], tuple[float, float, float]]:
    """Resolve R/G/B wavelength targets to available detector bands."""
    targets = tuple(float(value) for value in targets_nm)
    if len(targets) != 3:
        raise ValueError("False-color preview requires exactly three wavelengths")
    if band_count < 3:
        raise ValueError("At least three spectral bands are required")

    available = None if wavelengths is None else np.asarray(wavelengths, dtype=np.float32)
    if available is None or available.ndim != 1 or available.size != band_count:
        fallback = np.linspace(0, band_count - 1, 3, dtype=np.int64)[::-1]
        return tuple(int(index) for index in fallback), targets

    indices = tuple(
        int(np.argmin(np.abs(available - target)))
        for target in targets
    )
    actual = tuple(float(available[index]) for index in indices)
    return indices, actual


def false_color_lines(
    corrected_block: np.ndarray,
    band_indices: Sequence[int],
    *,
    reflectance_min: float = 0.0,
    reflectance_max: float = 0.9,
    gamma: float = 1.0,
) -> np.ndarray:
    """Convert calibrated spectral lines to rolling RGB display lines."""
    block = np.asarray(corrected_block, dtype=np.float32)
    indices = tuple(int(index) for index in band_indices)
    if block.ndim != 3:
        raise ValueError("Expected corrected block shaped (lines, bands, width)")
    if len(indices) != 3:
        raise ValueError("False-color preview requires exactly three band indices")
    if any(index < 0 or index >= block.shape[1] for index in indices):
        raise ValueError("False-color band index is outside the detector range")
    if reflectance_max <= reflectance_min:
        raise ValueError("reflectance_max must be greater than reflectance_min")
    if gamma <= 0:
        raise ValueError("gamma must be positive")

    selected = block[:, indices, :].transpose(0, 2, 1)
    normalized = np.clip(
        (selected - reflectance_min) / (reflectance_max - reflectance_min),
        0.0,
        1.0,
    )
    if gamma != 1.0:
        normalized = np.power(normalized, 1.0 / gamma)
    return np.round(normalized * 255.0).astype(np.uint8)


def detector_image(
    raw_line: np.ndarray,
    *,
    percentile_low: float = 1.0,
    percentile_high: float = 99.0,
    high_wavelength_at_top: bool = True,
) -> np.ndarray:
    """Render one raw FX17 detector frame as a contrast-normalized RGB image."""
    image, _ = detector_image_with_range(
        raw_line,
        percentile_low=percentile_low,
        percentile_high=percentile_high,
        high_wavelength_at_top=high_wavelength_at_top,
    )
    return image


def detector_image_with_range(
    raw_line: np.ndarray,
    *,
    percentile_low: float = 1.0,
    percentile_high: float = 99.0,
    high_wavelength_at_top: bool = True,
) -> tuple[np.ndarray, tuple[float, float]]:
    """Render a detector frame and return the displayed raw-value range."""
    frame = np.asarray(raw_line, dtype=np.float32)
    if frame.ndim != 2:
        raise ValueError("Expected one detector frame shaped (bands, width)")
    if not 0.0 <= percentile_low < percentile_high <= 100.0:
        raise ValueError("Invalid detector contrast percentiles")

    finite = frame[np.isfinite(frame)]
    if finite.size == 0:
        low, high = 0.0, 1.0
        gray = np.zeros(frame.shape, dtype=np.uint8)
    else:
        low, high = np.percentile(finite, [percentile_low, percentile_high])
        if high <= low:
            high = low + 1.0
        normalized = np.clip((frame - low) / (high - low), 0.0, 1.0)
        gray = np.round(
            np.nan_to_num(normalized, nan=0.0, posinf=1.0, neginf=0.0) * 255.0
        ).astype(np.uint8)

    if high_wavelength_at_top:
        gray = gray[::-1]
    return (
        np.repeat(gray[:, :, None], 3, axis=2),
        (float(low), float(high)),
    )
