from __future__ import annotations

import json
import queue
import sys
import threading
import time
import traceback
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path
from types import SimpleNamespace
from typing import Literal

import numpy as np

from analysis_session_storage import SampleOutputTarget
from sample_record import (
    update_sample_record,
    write_sample_record,
    write_sample_record_bundle,
    write_sample_record_pair,
)


APP_DIR = Path(__file__).resolve().parent
WORKSPACE = APP_DIR.parents[1]
DEFAULT_CAE_RUN_DIR = (
    WORKSPACE
    / "CAE訓練"
    / "cae_runs"
    / "cae_site16_ft_4fold_oof_50ep"
    / "final_model"
)
DEFAULT_CAE_CALIBRATION = DEFAULT_CAE_RUN_DIR / "site16_oof_calibration.json"
DEFAULT_RESISTANCE_MODEL = (
    WORKSPACE
    / "電阻值模型"
    / "resistance_model_no_thickness_v3_outputs"
    / "resistance_model_no_thickness.npz"
)

MASK_FALLBACK_MIN_SAMPLE_FRACTION = 0.25
MASK_FALLBACK_MAX_COMPONENTS = 8
MASK_FALLBACK_MIN_LARGEST_COMPONENT_FRACTION = 0.80
MASK_FALLBACK_PADDING = 32
MASK_WARNING_MIN_SAMPLE_FRACTION = 0.10
MASK_WARNING_MIN_LABEL_PIXELS = 500


def spatial_roi_bounds(
    width: int,
    start: int = 0,
    end: int | None = None,
) -> tuple[int, int]:
    roi_start = int(start)
    roi_end = int(width) if end is None else int(end)
    if not 0 <= roi_start < roi_end <= int(width):
        raise ValueError(
            "Expected 0 <= spatial_roi_start < spatial_roi_end <= cube width"
        )
    return roi_start, roi_end


def _component_summary(mask: np.ndarray) -> tuple[int, float]:
    """Return connected-component count and largest-component pixel fraction."""
    import cv2

    count, _, stats, _ = cv2.connectedComponentsWithStats(
        np.asarray(mask, dtype=np.uint8), connectivity=8
    )
    if count <= 1:
        return 0, 0.0
    sizes = stats[1:, cv2.CC_STAT_AREA].astype(np.int64)
    return int(sizes.size), float(sizes.max() / max(int(sizes.sum()), 1))


def _keep_significant_components(
    mask: np.ndarray,
    *,
    minimum_pixels: int = 64,
    minimum_largest_fraction: float = 0.01,
) -> tuple[np.ndarray, dict]:
    """Discard tiny isolated mask islands while preserving meaningful paths."""
    import cv2

    count, labels, stats, _ = cv2.connectedComponentsWithStats(
        np.asarray(mask, dtype=np.uint8), connectivity=8
    )
    if count <= 1:
        return np.asarray(mask, dtype=bool), {
            "components_before_cleanup": 0,
            "components_after_cleanup": 0,
            "largest_component_fraction_before_cleanup": 0.0,
        }

    sizes = stats[1:, cv2.CC_STAT_AREA].astype(np.int64)
    largest = int(sizes.max())
    threshold = max(
        int(minimum_pixels),
        int(np.ceil(largest * float(minimum_largest_fraction))),
    )
    kept_labels = np.flatnonzero(sizes >= threshold) + 1
    cleaned = np.isin(labels, kept_labels)
    return cleaned.astype(bool), {
        "components_before_cleanup": int(sizes.size),
        "components_after_cleanup": int(kept_labels.size),
        "largest_component_fraction_before_cleanup": float(
            largest / max(int(sizes.sum()), 1)
        ),
        "component_cleanup_min_pixels": int(threshold),
    }


def _keep_largest_8_connected_component(
    mask: np.ndarray,
) -> tuple[np.ndarray, dict]:
    """Keep exactly the largest 8-connected mask component.

    The site fine-tune and OOF calibration used this same final mask strategy.
    Applying it here after both direct and fallback masking keeps live residual,
    uniformity, and resistance calculations consistent with training.
    """
    import cv2

    count, labels, stats, _ = cv2.connectedComponentsWithStats(
        np.asarray(mask, dtype=np.uint8), connectivity=8
    )
    if count <= 1:
        return np.asarray(mask, dtype=bool), {
            "components_before_largest": 0,
            "component_sizes_desc": [],
            "largest_component_pixels": 0,
            "removed_component_pixels": 0,
            "largest_component_fraction": 0.0,
            "mask_strategy_final": "largest_8_connected_only",
        }

    sizes = stats[1:, cv2.CC_STAT_AREA].astype(np.int64)
    largest_index = int(np.argmax(sizes))
    largest_label = largest_index + 1
    largest_pixels = int(sizes[largest_index])
    cleaned = labels == largest_label
    return cleaned.astype(bool), {
        "components_before_largest": int(sizes.size),
        "component_sizes_desc": sorted(
            (int(value) for value in sizes), reverse=True
        ),
        "largest_component_pixels": largest_pixels,
        "removed_component_pixels": int(sizes.sum() - largest_pixels),
        "largest_component_fraction": float(
            largest_pixels / max(int(sizes.sum()), 1)
        ),
        "mask_strategy_final": "largest_8_connected_only",
    }


def _find_label_mask_from_sample_mask(
    cube: np.ndarray,
    run_args,
    sample_mask: np.ndarray,
) -> tuple[np.ndarray, dict]:
    """Apply the training mask logic using a supplied material/substrate mask."""
    from auto_sam_mask import (
        choose_label_seed,
        choose_sam_mask,
        normalize01,
        sam_degrees,
    )

    mean_image = cube.mean(axis=2)
    signal = normalize01(mean_image, sample_mask)
    seed, polarity, seed_info = choose_label_seed(
        signal=signal,
        sample_mask=sample_mask,
        label_polarity=run_args.label_polarity,
        seed_percentile=run_args.seed_percentile,
    )
    reference = cube[seed].mean(axis=0)
    sam = sam_degrees(cube, reference)
    label_mask, sam_threshold = choose_sam_mask(
        sam=sam,
        sample_mask=sample_mask,
        threshold_deg=run_args.sam_threshold_deg,
        max_label_fraction=run_args.max_label_fraction,
    )
    meta = {
        "label_polarity": polarity,
        "sam_threshold_deg": float(sam_threshold),
        "seed_pixels": int(seed_info["seed_pixels"]),
        "sample_pixels": int(sample_mask.sum()),
        "label_pixels": int(label_mask.sum()),
        "label_fraction_of_sample": float(
            label_mask.sum() / max(sample_mask.sum(), 1)
        ),
    }
    return label_mask.astype(bool), meta


def _background_padded_sample_mask(
    mean_image: np.ndarray,
    *,
    padding: int = MASK_FALLBACK_PADDING,
) -> np.ndarray:
    """Recover a bright substrate when it fills nearly the complete ROI."""
    from auto_sam_mask import build_sample_mask

    pad = max(1, int(padding))
    padded = np.pad(
        np.asarray(mean_image, dtype=np.float32),
        ((pad, pad), (pad, pad)),
        mode="constant",
        constant_values=0.0,
    )
    padded_mask = build_sample_mask(padded)
    return np.asarray(padded_mask[pad:-pad, pad:-pad], dtype=bool)


def find_label_mask_in_spatial_roi(
    cube: np.ndarray,
    find_label_mask,
    run_args,
    *,
    start: int = 0,
    end: int | None = None,
) -> tuple[np.ndarray, dict]:
    """Run automatic masking only inside the conveyor ROI."""
    roi_start, roi_end = spatial_roi_bounds(cube.shape[1], start, end)
    roi_cube = cube[:, roi_start:roi_end, :]
    expected = (cube.shape[0], roi_end - roi_start)
    roi_area = int(np.prod(expected))

    direct_error: Exception | None = None
    try:
        roi_mask, meta = find_label_mask(roi_cube, run_args)
        if roi_mask.shape != expected:
            raise ValueError(
                f"ROI label mask shape {roi_mask.shape} does not match {expected}."
            )
        meta = dict(meta)
    except (IndexError, ValueError) as exc:
        # A sample that fills the ROI can be mistaken for an edge band, leaving
        # no pixels for percentile/SAM selection. Retry with an artificial dark
        # border so the substrate boundary remains visible to the mask builder.
        direct_error = exc
        roi_mask = np.zeros(expected, dtype=bool)
        meta = {"sample_pixels": 0, "label_pixels": 0}

    sample_pixels = int(meta.get("sample_pixels", roi_area))
    label_pixels = int(roi_mask.sum())
    sample_fraction = float(sample_pixels / max(roi_area, 1))
    component_count, largest_fraction = _component_summary(roi_mask)
    suspicious = direct_error is not None or (
        sample_fraction < MASK_FALLBACK_MIN_SAMPLE_FRACTION
    )

    if suspicious:
        fallback_sample_mask = _background_padded_sample_mask(
            roi_cube.mean(axis=2),
        )
        fallback_mask, fallback_meta = _find_label_mask_from_sample_mask(
            roi_cube,
            run_args,
            fallback_sample_mask,
        )
        fallback_mask, cleanup_meta = _keep_significant_components(fallback_mask)
        fallback_components, fallback_largest_fraction = _component_summary(
            fallback_mask
        )
        fallback_sample_pixels = int(fallback_sample_mask.sum())
        fallback_expands_substrate = fallback_sample_pixels >= max(
            int(sample_pixels * 1.5),
            int(roi_area * 0.40),
        )
        fallback_is_connected = (
            fallback_components <= MASK_FALLBACK_MAX_COMPONENTS
            and fallback_largest_fraction
            >= MASK_FALLBACK_MIN_LARGEST_COMPONENT_FRACTION
        )
        if fallback_expands_substrate and fallback_is_connected:
            roi_mask = fallback_mask
            meta = dict(fallback_meta)
            meta.update(cleanup_meta)
            meta.update(
                {
                    "mask_strategy": "padded_background_fallback",
                    "fallback_padding": MASK_FALLBACK_PADDING,
                    "initial_sample_pixels": sample_pixels,
                    "initial_sample_fraction_of_roi": sample_fraction,
                    "initial_label_pixels": label_pixels,
                    "initial_label_components": component_count,
                    "initial_largest_component_fraction": largest_fraction,
                    "label_pixels": int(roi_mask.sum()),
                    "label_fraction_of_sample": float(
                        roi_mask.sum() / max(fallback_sample_pixels, 1)
                    ),
                }
            )
            if direct_error is not None:
                meta["initial_mask_error"] = str(direct_error)
        else:
            meta["mask_strategy"] = "direct"
            if direct_error is not None:
                raise RuntimeError(
                    "Automatic sample masking failed and the padded fallback "
                    "did not produce a usable mask."
                ) from direct_error
    else:
        meta["mask_strategy"] = "direct"

    # Always match the final mask used to fine-tune and calibrate the 50-epoch
    # site model.  Previously this cleanup only happened in one fallback path,
    # so direct SAM masks could retain isolated 1-3 pixel islands.
    roi_mask, largest_meta = _keep_largest_8_connected_component(roi_mask)
    meta.update(largest_meta)

    final_components, final_largest_fraction = _component_summary(roi_mask)
    meta.update(
        {
            "label_pixels": int(roi_mask.sum()),
            "label_components": final_components,
            "largest_component_fraction": final_largest_fraction,
        }
    )
    label_mask = np.zeros(cube.shape[:2], dtype=bool)
    label_mask[:, roi_start:roi_end] = roi_mask
    meta.update(
        {
            "spatial_roi_start": roi_start,
            "spatial_roi_end": roi_end,
            "spatial_roi_width": roi_end - roi_start,
        }
    )
    return label_mask, meta


def _safe_sample_stem(sample_id: str) -> str:
    stem = "".join(
        character if character.isalnum() or character in "-_" else "_"
        for character in str(sample_id)
    ).strip("_./ ")
    return stem or "sample"


def save_annotated_heatmap(
    path: str | Path,
    heatmap: np.ndarray,
    *,
    uniformity: float,
    resistance_ohm: float,
) -> Path:
    """Save an RGB heatmap with the two report metrics embedded in the image."""
    import cv2

    output_path = Path(path).resolve()
    output_path.parent.mkdir(parents=True, exist_ok=True)
    image = np.ascontiguousarray(heatmap, dtype=np.uint8)
    if image.ndim != 3 or image.shape[2] != 3:
        raise ValueError(f"Expected RGB heatmap shape (height, width, 3), got {image.shape}")
    header_height = max(52, int(round(image.shape[1] * 0.075)))
    canvas = np.zeros(
        (image.shape[0] + header_height, image.shape[1], 3),
        dtype=np.uint8,
    )
    canvas[:header_height] = (24, 34, 48)
    canvas[header_height:] = image
    text = f"Uniformity: {uniformity:.1f}%   Resistance: {resistance_ohm:.2f} ohm"
    font_scale = max(0.55, min(1.1, image.shape[1] / 760.0))
    cv2.putText(
        canvas,
        text,
        (16, int(header_height * 0.68)),
        cv2.FONT_HERSHEY_SIMPLEX,
        font_scale,
        (255, 255, 255),
        max(1, int(round(font_scale * 2))),
        cv2.LINE_AA,
    )
    success, encoded = cv2.imencode(".png", canvas[:, :, ::-1])
    if not success:
        raise IOError(f"Failed to encode heatmap: {output_path}")
    temporary = output_path.with_name(f".{output_path.name}.tmp")
    try:
        temporary.write_bytes(encoded.tobytes())
        temporary.replace(output_path)
    finally:
        temporary.unlink(missing_ok=True)
    return output_path


@dataclass(frozen=True)
class LiveAnalysisResult:
    sample_id: str
    uniformity: float
    resistance_ohm: float
    heatmap: np.ndarray
    mean_residual: float
    p95_residual: float
    hot_area_ratio: float
    label_pixels: int
    sample_lines: int
    elapsed_seconds: float
    device: str
    quality_warnings: tuple[str, ...] = ()
    acquisition_integrity: dict | None = None
    capture_path: str | None = None
    sample_record_path: str | None = None
    record_id: str | None = None
    sample_output_dir: str | None = None
    heatmap_path: str | None = None


@dataclass(frozen=True)
class WorkerMessage:
    kind: Literal["ready", "result", "error"]
    sample_id: str | None = None
    result: LiveAnalysisResult | None = None
    error: str | None = None
    output_target: SampleOutputTarget | None = None


@dataclass(frozen=True)
class _AnalysisTask:
    sample_id: str
    cube: np.ndarray | None = None
    raw_cube: np.ndarray | None = None
    hdr_path: Path | None = None
    acquisition_metadata: dict | None = None
    output_target: SampleOutputTarget | None = None


class LiveModelAnalyzer:
    """Persistent CAE and resistance inference models for complete samples."""

    def __init__(
        self,
        *,
        cae_run_dir: str | Path = DEFAULT_CAE_RUN_DIR,
        cae_calibration_path: str | Path = DEFAULT_CAE_CALIBRATION,
        resistance_model_path: str | Path = DEFAULT_RESISTANCE_MODEL,
        device: str = "cuda",
        spatial_roi_start: int = 0,
        spatial_roi_end: int | None = None,
        capture_dir: str | Path | None = None,
        capture_limit: int = 0,
        sample_record_dir: str | Path | None = None,
        sample_wavelengths_nm: list[float] | None = None,
        raw_sample_wavelengths_nm: list[float] | None = None,
        sample_sensor_type: str = "Specim FX17e",
    ) -> None:
        self.cae_run_dir = Path(cae_run_dir).resolve()
        self.cae_calibration_path = Path(cae_calibration_path).resolve()
        self.resistance_model_path = Path(resistance_model_path).resolve()
        self.spatial_roi_start = int(spatial_roi_start)
        self.spatial_roi_end = (
            None if spatial_roi_end is None else int(spatial_roi_end)
        )
        self.capture_dir = (
            None if capture_dir is None else Path(capture_dir).resolve()
        )
        self.capture_limit = max(0, int(capture_limit))
        self.capture_count = 0
        self.sample_record_dir = (
            None
            if sample_record_dir is None
            else Path(sample_record_dir).resolve()
        )
        self.sample_wavelengths_nm = (
            None
            if sample_wavelengths_nm is None
            else [float(value) for value in sample_wavelengths_nm]
        )
        self.raw_sample_wavelengths_nm = (
            None
            if raw_sample_wavelengths_nm is None
            else [float(value) for value in raw_sample_wavelengths_nm]
        )
        self.sample_sensor_type = str(sample_sensor_type)

        cae_dir = str(WORKSPACE / "CAE訓練")
        resistance_dir = str(WORKSPACE / "電阻值模型")
        for path in (resistance_dir, cae_dir):
            if path not in sys.path:
                sys.path.insert(0, path)

        import torch
        from auto_sam_mask import load_envi_bil
        from infer_cae import load_model, load_pca, load_run_args
        from predict_resistance import (
            load_model as load_resistance_model,
            predict as predict_resistance,
        )
        from train_cae import (
            find_label_mask,
            residual_map_for_image,
            residual_to_heatmap,
            transform_cube_to_pca,
        )

        if device.startswith("cuda") and not torch.cuda.is_available():
            build = (
                "CPU-only"
                if torch.version.cuda is None
                else f"CUDA {torch.version.cuda}"
            )
            raise RuntimeError(
                "CUDA was requested, but PyTorch cannot access a CUDA device. "
                f"Installed PyTorch: {torch.__version__} ({build}). On this "
                "project's CUDA 12.6 Windows environment, install "
                "requirements-cuda126.txt with the same Python that runs app.py."
            )
        self.torch = torch
        self.device = torch.device(device)
        self.load_envi_bil = load_envi_bil
        self.find_label_mask = find_label_mask
        self.transform_cube_to_pca = transform_cube_to_pca
        self.residual_map_for_image = residual_map_for_image
        self.residual_to_heatmap = residual_to_heatmap
        self.predict_resistance = predict_resistance

        cli_args = SimpleNamespace(data_dir=None, device=device, residual_space=None)
        self.run_args = load_run_args(self.cae_run_dir, cli_args)
        self.pca = load_pca(self.cae_run_dir)
        self.cae_model = load_model(self.cae_run_dir, self.device)
        self.resistance_model, self.resistance_metadata = load_resistance_model(
            self.resistance_model_path
        )
        calibration = json.loads(
            self.cae_calibration_path.read_text(encoding="utf-8")
        )
        self.calibration_p99 = float(calibration["calibration_pixel_p99"])

        feature_names = [
            str(value) for value in self.resistance_model["feature_names"].tolist()
        ]
        if not feature_names or any(
            not name.startswith("spec_") for name in feature_names
        ):
            raise ValueError(
                "The live pipeline requires the no-thickness spectrum-only resistance model."
            )
        self.resistance_feature_indices = np.array(
            [int(name.split("_", 1)[1]) for name in feature_names],
            dtype=np.int64,
        )

    def load_cube(self, hdr_path: str | Path) -> np.ndarray:
        cube, _, _ = self.load_envi_bil(
            Path(hdr_path),
            trim_bands=int(self.run_args.trim_bands),
        )
        return np.ascontiguousarray(cube, dtype=np.float32)

    def analyze_cube(
        self,
        sample_id: str,
        cube: np.ndarray,
        *,
        raw_cube: np.ndarray | None = None,
        acquisition_metadata: dict | None = None,
        output_target: SampleOutputTarget | None = None,
    ) -> LiveAnalysisResult:
        sample_record_stems = self._record_sample_input(
            sample_id,
            cube,
            raw_cube=raw_cube,
            acquisition_metadata=acquisition_metadata,
            output_target=output_target,
        )
        try:
            return self._analyze_cube_impl(
                sample_id,
                cube,
                sample_record_stems=sample_record_stems,
                acquisition_metadata=acquisition_metadata,
                output_target=output_target,
            )
        except Exception:
            if sample_record_stems:
                try:
                    for sample_record_stem in sample_record_stems:
                        update_sample_record(
                            sample_record_stem,
                            {
                                "status": "analysis_failed",
                                "analysis": {
                                    "status": "failed",
                                    "error": traceback.format_exc(),
                                },
                            },
                        )
                except Exception:
                    traceback.print_exc()
            raise

    def _analyze_cube_impl(
        self,
        sample_id: str,
        cube: np.ndarray,
        *,
        sample_record_stems: tuple[Path, ...],
        acquisition_metadata: dict | None = None,
        output_target: SampleOutputTarget | None = None,
    ) -> LiveAnalysisResult:
        started = time.perf_counter()
        cube = np.asarray(cube, dtype=np.float32)
        if cube.ndim != 3:
            raise ValueError(
                f"Expected a (lines, width, bands) cube, received {cube.shape}."
            )
        expected_bands = int(self.pca.band_mean.shape[0])
        if cube.shape[2] != expected_bands:
            raise ValueError(
                f"Expected {expected_bands} trimmed bands, received {cube.shape[2]}."
            )
        if not np.isfinite(cube).all():
            raise ValueError("Input cube contains non-finite values.")

        capture_stem = self._capture_input(sample_id, cube)
        label_mask, mask_meta = find_label_mask_in_spatial_roi(
            cube,
            self.find_label_mask,
            self.run_args,
            start=self.spatial_roi_start,
            end=self.spatial_roi_end,
        )
        if not label_mask.any():
            raise RuntimeError(f"No conductive-material pixels found for {sample_id}.")

        roi_area = int(cube.shape[0] * int(mask_meta["spatial_roi_width"]))
        sample_pixels = int(mask_meta.get("sample_pixels", roi_area))
        quality_warnings: list[str] = []
        if sample_pixels < int(roi_area * MASK_WARNING_MIN_SAMPLE_FRACTION):
            quality_warnings.append("樣本板遮罩面積偏小")
        if int(mask_meta["label_pixels"]) < MASK_WARNING_MIN_LABEL_PIXELS:
            quality_warnings.append("導電線遮罩像素偏少")

        acquisition_integrity = None
        if isinstance(acquisition_metadata, dict):
            acquisition = acquisition_metadata.get("acquisition")
            if isinstance(acquisition, dict):
                integrity = acquisition.get("integrity")
                if isinstance(integrity, dict):
                    acquisition_integrity = dict(integrity)

        pca_cube = self.transform_cube_to_pca(cube, self.pca)
        residual = self.residual_map_for_image(
            self.cae_model,
            cube,
            pca_cube,
            label_mask,
            self.pca,
            self.run_args,
            self.device,
        )
        finite = np.isfinite(residual) & label_mask
        if not finite.any():
            raise RuntimeError(f"No finite CAE residual pixels found for {sample_id}.")

        residual_values = residual[finite]
        mean_residual = float(np.mean(residual_values))
        p95_residual = float(np.percentile(residual_values, 95))
        hot_area_ratio = float(np.mean(residual_values > self.calibration_p99))
        normalized_error = min(
            mean_residual / max(self.calibration_p99, 1e-12),
            1.0,
        )
        uniformity = 100.0 * (1.0 - normalized_error)
        heatmap, _ = self.residual_to_heatmap(residual, label_mask)

        # Match the float32 reduction used when the resistance training cache was built.
        mean_spectrum = cube[label_mask].mean(axis=0).astype(np.float32)
        if self.resistance_feature_indices.max(initial=-1) >= mean_spectrum.size:
            raise ValueError("Resistance model references a missing spectral band.")
        features = mean_spectrum[self.resistance_feature_indices].astype(
            np.float64
        ).reshape(1, -1)
        resistance = float(
            self.predict_resistance(self.resistance_model, features).reshape(-1)[0]
        )
        heatmap_path = None
        if output_target is not None:
            heatmap_path = save_annotated_heatmap(
                output_target.heatmap_path,
                heatmap,
                uniformity=uniformity,
                resistance_ohm=resistance,
            )
        self._capture_analysis(
            capture_stem,
            label_mask=label_mask,
            residual=residual,
            heatmap=heatmap,
            uniformity=uniformity,
            resistance_ohm=resistance,
            mean_residual=mean_residual,
            p95_residual=p95_residual,
            hot_area_ratio=hot_area_ratio,
            mask_meta=mask_meta,
        )

        if self.device.type == "cuda":
            self.torch.cuda.synchronize(self.device)
        elapsed_seconds = time.perf_counter() - started
        analysis_record = {
            "status": "analyzed",
            "analysis": {
                "status": "completed",
                "uniformity_percent": uniformity,
                "resistance_ohm": resistance,
                "mean_residual": mean_residual,
                "p95_residual": p95_residual,
                "hot_area_ratio": hot_area_ratio,
                "label_pixels": int(mask_meta["label_pixels"]),
                "sample_lines": int(cube.shape[0]),
                "elapsed_seconds": elapsed_seconds,
                "device": str(self.device),
                "mask": mask_meta,
                "quality_warnings": quality_warnings,
                "acquisition_integrity": acquisition_integrity,
                "calibration_pixel_p99": self.calibration_p99,
                "cae_run_dir": str(self.cae_run_dir),
                "cae_calibration_path": str(self.cae_calibration_path),
                "resistance_model_path": str(self.resistance_model_path),
                "heatmap_path": (
                    None if heatmap_path is None else str(heatmap_path)
                ),
                "sample_output_dir": (
                    None if output_target is None else str(output_target.sample_dir)
                ),
                "capture_timestamp": (
                    None
                    if output_target is None
                    else output_target.capture_timestamp
                ),
            },
        }
        for sample_record_stem in sample_record_stems:
            update_sample_record(
                sample_record_stem,
                analysis_record,
            )
        return LiveAnalysisResult(
            sample_id=sample_id,
            uniformity=uniformity,
            resistance_ohm=resistance,
            heatmap=np.ascontiguousarray(heatmap, dtype=np.uint8),
            mean_residual=mean_residual,
            p95_residual=p95_residual,
            hot_area_ratio=hot_area_ratio,
            label_pixels=int(mask_meta["label_pixels"]),
            sample_lines=int(cube.shape[0]),
            elapsed_seconds=elapsed_seconds,
            device=str(self.device),
            quality_warnings=tuple(quality_warnings),
            acquisition_integrity=acquisition_integrity,
            capture_path=(
                None if capture_stem is None else str(capture_stem.with_suffix(".npy"))
            ),
            sample_record_path=(
                None
                if not sample_record_stems
                else str(sample_record_stems[-1].with_suffix(".hdr"))
            ),
            record_id=(None if output_target is None else output_target.record_id),
            sample_output_dir=(
                None if output_target is None else str(output_target.sample_dir)
            ),
            heatmap_path=(None if heatmap_path is None else str(heatmap_path)),
        )

    def _record_sample_input(
        self,
        sample_id: str,
        cube: np.ndarray,
        *,
        raw_cube: np.ndarray | None,
        acquisition_metadata: dict | None,
        output_target: SampleOutputTarget | None,
    ) -> tuple[Path, ...]:
        if output_target is not None:
            if raw_cube is None:
                calibrated_stem = write_sample_record(
                    output_target.sample_dir,
                    stem_name=f"{output_target.file_stem}_校正",
                    sample_id=sample_id,
                    cube=cube,
                    acquisition_metadata=acquisition_metadata,
                    wavelengths_nm=self.sample_wavelengths_nm,
                    sensor_type=self.sample_sensor_type,
                )
                return (calibrated_stem,)
            return write_sample_record_bundle(
                output_target.sample_dir,
                file_stem=output_target.file_stem,
                sample_id=sample_id,
                uncalibrated_cube=raw_cube,
                calibrated_cube=cube,
                acquisition_metadata=acquisition_metadata,
                uncalibrated_wavelengths_nm=self.raw_sample_wavelengths_nm,
                calibrated_wavelengths_nm=self.sample_wavelengths_nm,
                sensor_type=self.sample_sensor_type,
            )
        if self.sample_record_dir is None:
            return ()
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S_%f")
        stem_name = f"{timestamp}_{_safe_sample_stem(sample_id)}"
        if raw_cube is None:
            calibrated_stem = write_sample_record(
                self.sample_record_dir / "calibrated",
                stem_name=stem_name,
                sample_id=sample_id,
                cube=cube,
                acquisition_metadata=acquisition_metadata,
                wavelengths_nm=self.sample_wavelengths_nm,
                sensor_type=self.sample_sensor_type,
            )
            return (calibrated_stem,)
        return write_sample_record_pair(
            self.sample_record_dir,
            stem_name=stem_name,
            sample_id=sample_id,
            uncalibrated_cube=raw_cube,
            calibrated_cube=cube,
            acquisition_metadata=acquisition_metadata,
            uncalibrated_wavelengths_nm=self.raw_sample_wavelengths_nm,
            calibrated_wavelengths_nm=self.sample_wavelengths_nm,
            sensor_type=self.sample_sensor_type,
        )

    def _capture_input(
        self,
        sample_id: str,
        cube: np.ndarray,
    ) -> Path | None:
        if (
            self.capture_dir is None
            or self.capture_count >= self.capture_limit
        ):
            return None
        try:
            self.capture_dir.mkdir(parents=True, exist_ok=True)
            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S_%f")
            stem = self.capture_dir / f"{timestamp}_{_safe_sample_stem(sample_id)}"
            with stem.with_suffix(".npy").open("wb") as handle:
                np.save(
                    handle,
                    np.ascontiguousarray(cube, dtype=np.float32),
                    allow_pickle=False,
                )
            roi_start, roi_end = spatial_roi_bounds(
                cube.shape[1],
                self.spatial_roi_start,
                self.spatial_roi_end,
            )
            metadata = {
                "sample_id": sample_id,
                "shape": [int(value) for value in cube.shape],
                "dtype": "float32",
                "content": "black-white corrected analysis input cube",
                "axis_order": ["scan_line", "spatial_pixel", "spectral_band"],
                "spatial_roi_start": roi_start,
                "spatial_roi_end": roi_end,
                "calibration_pixel_p99": self.calibration_p99,
            }
            stem.with_suffix(".json").write_text(
                json.dumps(metadata, ensure_ascii=False, indent=2),
                encoding="utf-8",
            )
            self.capture_count += 1
            return stem
        except Exception:
            traceback.print_exc()
            return None

    @staticmethod
    def _capture_analysis(
        capture_stem: Path | None,
        **values,
    ) -> None:
        if capture_stem is None:
            return
        try:
            mask_meta = values.pop("mask_meta")
            np.savez_compressed(
                capture_stem.with_name(capture_stem.name + "_analysis.npz"),
                **values,
            )
            metadata_path = capture_stem.with_suffix(".json")
            metadata = json.loads(metadata_path.read_text(encoding="utf-8"))
            metadata["mask"] = mask_meta
            metadata["uniformity"] = float(values["uniformity"])
            metadata["resistance_ohm"] = float(values["resistance_ohm"])
            metadata["mean_residual"] = float(values["mean_residual"])
            metadata["p95_residual"] = float(values["p95_residual"])
            metadata["hot_area_ratio"] = float(values["hot_area_ratio"])
            metadata_path.write_text(
                json.dumps(metadata, ensure_ascii=False, indent=2),
                encoding="utf-8",
            )
        except Exception:
            traceback.print_exc()


class LiveAnalysisWorker:
    """Single persistent background worker; callers poll messages from the UI thread."""

    def __init__(
        self,
        *,
        cae_run_dir: str | Path = DEFAULT_CAE_RUN_DIR,
        cae_calibration_path: str | Path = DEFAULT_CAE_CALIBRATION,
        resistance_model_path: str | Path = DEFAULT_RESISTANCE_MODEL,
        device: str = "cuda",
        max_pending: int = 1,
        spatial_roi_start: int = 0,
        spatial_roi_end: int | None = None,
        capture_dir: str | Path | None = None,
        capture_limit: int = 0,
        sample_record_dir: str | Path | None = None,
        sample_wavelengths_nm: list[float] | None = None,
        raw_sample_wavelengths_nm: list[float] | None = None,
        sample_sensor_type: str = "Specim FX17e",
    ) -> None:
        self._config = {
            "cae_run_dir": cae_run_dir,
            "cae_calibration_path": cae_calibration_path,
            "resistance_model_path": resistance_model_path,
            "device": device,
            "spatial_roi_start": spatial_roi_start,
            "spatial_roi_end": spatial_roi_end,
            "capture_dir": capture_dir,
            "capture_limit": capture_limit,
            "sample_record_dir": sample_record_dir,
            "sample_wavelengths_nm": sample_wavelengths_nm,
            "raw_sample_wavelengths_nm": raw_sample_wavelengths_nm,
            "sample_sensor_type": sample_sensor_type,
        }
        self._tasks: queue.Queue[_AnalysisTask | None] = queue.Queue(
            maxsize=max_pending
        )
        self._messages: queue.Queue[WorkerMessage] = queue.Queue()
        self._closed = False
        self._thread = threading.Thread(
            target=self._run,
            name="live-model-analysis",
            daemon=True,
        )
        self._thread.start()

    def submit_cube(
        self,
        sample_id: str,
        cube: np.ndarray,
        *,
        raw_cube: np.ndarray | None = None,
        acquisition_metadata: dict | None = None,
        output_target: SampleOutputTarget | None = None,
        block: bool = False,
    ) -> bool:
        if self._closed:
            return False
        task = _AnalysisTask(
            sample_id=sample_id,
            cube=cube,
            raw_cube=raw_cube,
            acquisition_metadata=acquisition_metadata,
            output_target=output_target,
        )
        try:
            self._tasks.put(task, block=block)
        except queue.Full:
            return False
        return True

    def submit_hdr(
        self,
        sample_id: str,
        hdr_path: str | Path,
        *,
        block: bool = False,
    ) -> bool:
        if self._closed:
            return False
        task = _AnalysisTask(sample_id=sample_id, hdr_path=Path(hdr_path))
        try:
            self._tasks.put(task, block=block)
        except queue.Full:
            return False
        return True

    def poll(self) -> list[WorkerMessage]:
        messages: list[WorkerMessage] = []
        while True:
            try:
                messages.append(self._messages.get_nowait())
            except queue.Empty:
                return messages

    def close(self, timeout: float = 2.0) -> None:
        if self._closed:
            return
        self._closed = True
        while True:
            try:
                self._tasks.put_nowait(None)
                break
            except queue.Full:
                try:
                    self._tasks.get_nowait()
                except queue.Empty:
                    continue
        self._thread.join(timeout=timeout)

    def _run(self) -> None:
        try:
            analyzer = LiveModelAnalyzer(**self._config)
        except Exception:
            self._messages.put(
                WorkerMessage(
                    kind="error",
                    error="Model initialization failed:\n" + traceback.format_exc(),
                )
            )
            return

        self._messages.put(WorkerMessage(kind="ready"))
        while True:
            task = self._tasks.get()
            if task is None:
                return
            try:
                cube = (
                    task.cube
                    if task.cube is not None
                    else analyzer.load_cube(task.hdr_path)
                )
                result = analyzer.analyze_cube(
                    task.sample_id,
                    cube,
                    raw_cube=task.raw_cube,
                    acquisition_metadata=task.acquisition_metadata,
                    output_target=task.output_target,
                )
                self._messages.put(
                    WorkerMessage(
                        kind="result",
                        sample_id=task.sample_id,
                        result=result,
                        output_target=task.output_target,
                    )
                )
            except Exception:
                self._messages.put(
                    WorkerMessage(
                        kind="error",
                        sample_id=task.sample_id,
                        error=traceback.format_exc(),
                        output_target=task.output_target,
                    )
                )
