from __future__ import annotations

from dataclasses import dataclass, field

import numpy as np

try:
    import cv2
except ImportError:  # pragma: no cover - exercised only in minimal deployments
    cv2 = None


@dataclass(frozen=True)
class GeometryCalibrationConfig:
    projection_percentile_low: float = 1.0
    projection_percentile_high: float = 99.0
    min_area_fraction: float = 0.015
    max_area_fraction: float = 0.40
    border_margin_px: int = 5
    morphology_kernel_px: int = 7
    min_ellipse_fill: float = 0.85
    max_ellipse_fill: float = 1.15
    min_solidity: float = 0.90
    min_relative_contrast: float = 0.12
    min_axis_ratio: float = 0.25
    max_axis_ratio: float = 4.0
    max_axis_correlation: float = 0.20

    def validate(self) -> None:
        if not (
            0.0
            <= self.projection_percentile_low
            < self.projection_percentile_high
            <= 100.0
        ):
            raise ValueError("Invalid projection percentiles")
        if not 0.0 < self.min_area_fraction < self.max_area_fraction < 1.0:
            raise ValueError("Invalid circular-standard area limits")
        if self.border_margin_px < 0 or self.morphology_kernel_px < 1:
            raise ValueError("Invalid morphology or border settings")
        if self.min_ellipse_fill <= 0 or self.max_ellipse_fill <= self.min_ellipse_fill:
            raise ValueError("Invalid ellipse-fill limits")
        if not 0.0 < self.min_solidity <= 1.0:
            raise ValueError("Invalid minimum solidity")
        if self.min_relative_contrast <= 0:
            raise ValueError("Minimum contrast must be positive")
        if not 0.0 < self.min_axis_ratio < 1.0 < self.max_axis_ratio:
            raise ValueError("Invalid axis-ratio limits")
        if not 0.0 <= self.max_axis_correlation < 1.0:
            raise ValueError("Invalid axis-alignment limit")


@dataclass(frozen=True)
class GeometryCalibrationResult:
    width_px: float
    height_lines: float
    height_width_ratio: float
    line_rate_multiplier: float
    ellipse_fill: float
    solidity: float
    relative_contrast: float
    axis_correlation: float
    center_x: float
    center_y: float
    polarity: str
    preview: np.ndarray = field(repr=False)


@dataclass(frozen=True)
class _Candidate:
    score: float
    contour: np.ndarray
    ellipse: tuple
    mask: np.ndarray
    width_px: float
    height_lines: float
    ellipse_fill: float
    solidity: float
    center_x: float
    center_y: float
    polarity: str


def _normalize_projection(
    projection: np.ndarray,
    *,
    percentile_low: float,
    percentile_high: float,
) -> np.ndarray:
    finite = projection[np.isfinite(projection)]
    if finite.size == 0:
        raise ValueError("Circular-standard image contains no finite values")
    low, high = np.percentile(finite, (percentile_low, percentile_high))
    if high <= low:
        raise ValueError("Circular-standard image has no usable contrast")
    normalized = np.clip((projection - low) / (high - low), 0.0, 1.0)
    normalized = np.nan_to_num(normalized, nan=0.0, posinf=1.0, neginf=0.0)
    return np.round(normalized * 255.0).astype(np.uint8)


def _ellipse_extents(ellipse: tuple) -> tuple[float, float]:
    (center_x, center_y), (axis_a, axis_b), angle_degrees = ellipse
    angle = np.deg2rad(float(angle_degrees))
    radius_a = float(axis_a) / 2.0
    radius_b = float(axis_b) / 2.0
    half_width = np.sqrt(
        (radius_a * np.cos(angle)) ** 2
        + (radius_b * np.sin(angle)) ** 2
    )
    half_height = np.sqrt(
        (radius_a * np.sin(angle)) ** 2
        + (radius_b * np.cos(angle)) ** 2
    )
    return 2.0 * float(half_width), 2.0 * float(half_height)


def _find_candidate_opencv(
    image: np.ndarray,
    config: GeometryCalibrationConfig,
) -> _Candidate:
    image_height, image_width = image.shape
    frame_area = image_height * image_width
    best: _Candidate | None = None
    kernel_size = max(1, int(config.morphology_kernel_px))
    if kernel_size % 2 == 0:
        kernel_size += 1
    kernel = cv2.getStructuringElement(
        cv2.MORPH_ELLIPSE,
        (kernel_size, kernel_size),
    )

    for polarity, threshold_type in (
        ("bright", cv2.THRESH_BINARY),
        ("dark", cv2.THRESH_BINARY_INV),
    ):
        _, threshold = cv2.threshold(
            image,
            0,
            255,
            threshold_type | cv2.THRESH_OTSU,
        )
        threshold = cv2.morphologyEx(threshold, cv2.MORPH_OPEN, kernel)
        threshold = cv2.morphologyEx(threshold, cv2.MORPH_CLOSE, kernel)
        contours, _ = cv2.findContours(
            threshold,
            cv2.RETR_EXTERNAL,
            cv2.CHAIN_APPROX_NONE,
        )
        for contour in contours:
            area = float(cv2.contourArea(contour))
            if len(contour) < 20 or not (
                config.min_area_fraction * frame_area
                <= area
                <= config.max_area_fraction * frame_area
            ):
                continue
            x, y, width, height = cv2.boundingRect(contour)
            margin = config.border_margin_px
            if (
                x <= margin
                or y <= margin
                or x + width >= image_width - margin
                or y + height >= image_height - margin
            ):
                continue
            ellipse = cv2.fitEllipse(contour)
            width_px, height_lines = _ellipse_extents(ellipse)
            axis_a, axis_b = ellipse[1]
            ellipse_area = np.pi * float(axis_a) * float(axis_b) / 4.0
            ellipse_fill = area / max(ellipse_area, 1.0)
            hull_area = float(cv2.contourArea(cv2.convexHull(contour)))
            solidity = area / max(hull_area, 1.0)
            candidate_mask = np.zeros(image.shape, dtype=np.uint8)
            cv2.ellipse(candidate_mask, ellipse, 255, -1)
            candidate_mask = candidate_mask.astype(bool)
            score = area * solidity * np.exp(-3.0 * abs(1.0 - ellipse_fill))
            candidate = _Candidate(
                score=float(score),
                contour=contour,
                ellipse=ellipse,
                mask=candidate_mask,
                width_px=width_px,
                height_lines=height_lines,
                ellipse_fill=float(ellipse_fill),
                solidity=float(solidity),
                center_x=float(ellipse[0][0]),
                center_y=float(ellipse[0][1]),
                polarity=polarity,
            )
            if best is None or candidate.score > best.score:
                best = candidate

    if best is None:
        raise ValueError(
            "找不到完整且未碰觸影像邊界的圓形標準件；請確認圓形已完整通過。"
        )
    return best


def _axis_correlation(mask: np.ndarray) -> float:
    y, x = np.nonzero(mask)
    if x.size < 5:
        return 1.0
    x_centered = x.astype(np.float64) - float(np.mean(x))
    y_centered = y.astype(np.float64) - float(np.mean(y))
    denominator = np.sqrt(
        float(np.mean(x_centered * x_centered))
        * float(np.mean(y_centered * y_centered))
    )
    if denominator <= 1e-12:
        return 1.0
    return abs(float(np.mean(x_centered * y_centered)) / denominator)


def _relative_contrast(image: np.ndarray, mask: np.ndarray) -> float:
    if cv2 is None:
        outside = ~mask
    else:
        dilated = cv2.dilate(
            mask.astype(np.uint8),
            np.ones((31, 31), dtype=np.uint8),
        ).astype(bool)
        outside = dilated & ~mask
    if not mask.any() or not outside.any():
        return 0.0
    inside_value = float(np.median(image[mask]))
    outside_value = float(np.median(image[outside]))
    return abs(inside_value - outside_value) / 255.0


def _draw_preview(image: np.ndarray, candidate: _Candidate) -> np.ndarray:
    rgb = np.repeat(image[:, :, None], 3, axis=2)
    if cv2 is not None:
        cv2.drawContours(rgb, [candidate.contour], -1, (255, 230, 0), 2)
        cv2.ellipse(rgb, candidate.ellipse, (255, 64, 64), 2)
    return np.ascontiguousarray(rgb, dtype=np.uint8)


def estimate_geometry_calibration(
    cube: np.ndarray,
    config: GeometryCalibrationConfig | None = None,
) -> GeometryCalibrationResult:
    """Estimate push-broom scan-axis scaling from one flat circular standard."""
    config = config or GeometryCalibrationConfig()
    config.validate()
    sample = np.asarray(cube, dtype=np.float32)
    if sample.ndim != 3 or min(sample.shape) < 3:
        raise ValueError(
            f"Expected a (lines, width, bands) sample cube, received {sample.shape}."
        )
    finite_fraction = float(np.count_nonzero(np.isfinite(sample))) / sample.size
    if finite_fraction < 0.95:
        raise ValueError(
            f"Circular-standard cube has too many non-finite values "
            f"({finite_fraction:.1%} finite)"
        )
    band_margin = max(0, int(round(sample.shape[2] * 0.05)))
    band_stop = sample.shape[2] - band_margin
    selected = sample[:, :, band_margin:band_stop]
    projection = np.nanmedian(selected, axis=2).astype(np.float32, copy=False)
    image = _normalize_projection(
        projection,
        percentile_low=config.projection_percentile_low,
        percentile_high=config.projection_percentile_high,
    )
    if cv2 is None:
        raise RuntimeError(
            "形變校正需要 OpenCV；請安裝 requirements.txt 中的 opencv-python-headless。"
        )
    candidate = _find_candidate_opencv(image, config)
    ratio = candidate.height_lines / max(candidate.width_px, 1e-9)
    line_rate_multiplier = 1.0 / ratio
    contrast = _relative_contrast(image, candidate.mask)
    correlation = _axis_correlation(candidate.mask)

    problems: list[str] = []
    if not config.min_axis_ratio <= ratio <= config.max_axis_ratio:
        problems.append(f"H/W={ratio:.3f} 超出可接受範圍")
    if not config.min_ellipse_fill <= candidate.ellipse_fill <= config.max_ellipse_fill:
        problems.append(f"橢圓填合度={candidate.ellipse_fill:.3f} 不足")
    if candidate.solidity < config.min_solidity:
        problems.append(f"輪廓實心度={candidate.solidity:.3f} 不足")
    if contrast < config.min_relative_contrast:
        problems.append(f"圓形與背景對比={contrast:.3f} 不足")
    if correlation > config.max_axis_correlation:
        problems.append(f"圓形主軸偏斜（相關係數={correlation:.3f}）")
    if problems:
        raise ValueError("；".join(problems))

    preview = _draw_preview(image, candidate)
    return GeometryCalibrationResult(
        width_px=candidate.width_px,
        height_lines=candidate.height_lines,
        height_width_ratio=float(ratio),
        line_rate_multiplier=float(line_rate_multiplier),
        ellipse_fill=candidate.ellipse_fill,
        solidity=candidate.solidity,
        relative_contrast=float(contrast),
        axis_correlation=float(correlation),
        center_x=candidate.center_x,
        center_y=candidate.center_y,
        polarity=candidate.polarity,
        preview=preview,
    )
