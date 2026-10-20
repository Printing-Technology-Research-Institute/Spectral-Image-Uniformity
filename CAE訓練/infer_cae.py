from __future__ import annotations

import argparse
import csv
import json
import sys
from pathlib import Path
from types import SimpleNamespace

import numpy as np

from auto_sam_mask import EXCLUDED_FOR_TRAINING, load_envi_bil, pseudo_rgb
from train_cae import (
    PcaModel,
    discover_images,
    find_label_mask,
    load_torch_checkpoint,
    make_model,
    residual_map_for_image,
    resolve_data_dir,
    save_residual_images,
    save_result_summary,
    transform_cube_to_pca,
)

import torch


SCRIPT_DIR = Path(__file__).resolve().parent
def resolve_resistance_dir() -> Path:
    candidates = [SCRIPT_DIR.parent]
    candidates.extend(Path(value) for value in sys.path if value)
    seen: set[Path] = set()
    for candidate in candidates:
        try:
            candidate = candidate.resolve()
        except OSError:
            continue
        if candidate in seen:
            continue
        seen.add(candidate)
        if (candidate / "thickness_classifier.py").is_file():
            return candidate
        if not candidate.is_dir():
            continue
        for child in candidate.iterdir():
            if child.is_dir() and (child / "thickness_classifier.py").exists():
                return child
    raise FileNotFoundError("Cannot find the resistance model folder containing thickness_classifier.py")


RESISTANCE_DIR = resolve_resistance_dir()
if str(RESISTANCE_DIR) not in sys.path:
    sys.path.insert(0, str(RESISTANCE_DIR))

from thickness_classifier import (  # noqa: E402
    DEFAULT_THICKNESS_MODEL,
    load_thickness_classifier,
    parse_thickness_from_sample_id,
    predict_thickness_for_hdr,
)


DEFAULTS = {
    "trim_bands": 10,
    "label_polarity": "auto",
    "seed_percentile": 85.0,
    "sam_threshold_deg": None,
    "max_label_fraction": 0.32,
    "patch_size": 32,
    "eval_stride": 16,
    "include_mask_channel": True,
    "batch_size": 256,
    "amp": True,
    "residual_space": "spectral",
    "thickness": "all",
    "include_excluded": False,
}


def load_run_args(run_dir: Path, cli_args: argparse.Namespace) -> SimpleNamespace:
    config_path = run_dir / "config.json"
    if config_path.exists():
        config = json.loads(config_path.read_text(encoding="utf-8"))
        values = dict(config.get("args", {}))
    else:
        values = {}
    for key, value in DEFAULTS.items():
        values.setdefault(key, value)
    if cli_args.data_dir:
        values["data_dir"] = cli_args.data_dir
    if cli_args.device:
        values["device"] = cli_args.device
    if cli_args.residual_space:
        values["residual_space"] = cli_args.residual_space
    return SimpleNamespace(**values)


def load_pca(run_dir: Path) -> PcaModel:
    path = run_dir / "pca_model.npz"
    if not path.exists():
        raise FileNotFoundError(f"Missing PCA model: {path}")
    data = np.load(path)
    return PcaModel(
        band_mean=data["band_mean"].astype(np.float32),
        band_std=data["band_std"].astype(np.float32),
        components=data["components"].astype(np.float32),
        explained_variance_ratio=data["explained_variance_ratio"].astype(np.float32),
    )


def load_model(run_dir: Path, device):
    checkpoint_path = run_dir / "best_cae.pt"
    if not checkpoint_path.exists():
        raise FileNotFoundError(f"Missing trained model: {checkpoint_path}")
    checkpoint = load_torch_checkpoint(checkpoint_path, map_location=device)
    model = make_model(
        input_channels=int(checkpoint["input_channels"]),
        output_channels=int(checkpoint["output_channels"]),
        base_channels=int(checkpoint["base_channels"]),
    ).to(device)
    model.load_state_dict(checkpoint["model_state"])
    model.eval()
    return model


def calibration_from_metrics(run_dir: Path) -> float | None:
    metrics_path = run_dir / "residual_metrics.csv"
    if not metrics_path.exists():
        return None
    values = []
    with metrics_path.open("r", newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        for row in reader:
            try:
                value = float(row.get("calibration_pixel_p99", ""))
            except ValueError:
                continue
            if value > 0:
                values.append(value)
    if not values:
        return None
    return float(np.median(values))


def calibration_from_val_split(run_dir: Path, data_dir: Path, model, pca: PcaModel, args: SimpleNamespace, device) -> float:
    manifest_path = run_dir / "split_manifest.json"
    if not manifest_path.exists():
        raise FileNotFoundError(
            f"Cannot infer calibration p99 because {run_dir / 'residual_metrics.csv'} "
            f"and {manifest_path} are missing."
        )
    records = json.loads(manifest_path.read_text(encoding="utf-8"))
    values = []
    for record in records:
        if record.get("split") != "val":
            continue
        hdr_path = Path(record["hdr_path"])
        if not hdr_path.exists():
            hdr_path = data_dir / f"{record['stem']}.hdr"
        cube, _, _ = load_envi_bil(hdr_path, trim_bands=args.trim_bands)
        label_mask, _ = find_label_mask(cube, args)
        pca_cube = transform_cube_to_pca(cube, pca)
        residual = residual_map_for_image(model, cube, pca_cube, label_mask, pca, args, device)
        finite = np.isfinite(residual) & label_mask
        if finite.any():
            values.append(residual[finite])
    if not values:
        raise RuntimeError("No validation residual pixels were available for calibration.")
    return float(np.percentile(np.concatenate(values), 99))


def choose_inputs(data_dir: Path, cli_args: argparse.Namespace, run_args: SimpleNamespace | None = None) -> list[Path]:
    if cli_args.file:
        path = Path(cli_args.file)
        if not path.is_absolute():
            path = data_dir / path
        return [path]
    if cli_args.all:
        thickness = cli_args.thickness or (run_args.thickness if run_args is not None else "all")
        return discover_images(
            data_dir=data_dir,
            thickness=thickness,
            include_excluded=cli_args.include_excluded,
        )
    raise ValueError("Please pass --file <name.hdr> or --all.")


def infer_one(
    hdr_path: Path,
    output_root: Path,
    model,
    pca: PcaModel,
    args: SimpleNamespace,
    device,
    calibration_p99: float,
    output_name: str | None = None,
) -> dict:
    if hdr_path.stem in EXCLUDED_FOR_TRAINING and not getattr(args, "include_excluded", False):
        print(f"[skip] {hdr_path.name}: known HDR/RAW line mismatch")
        return {}

    cube, wavelengths, _ = load_envi_bil(hdr_path, trim_bands=args.trim_bands)
    label_mask, mask_meta = find_label_mask(cube, args)
    pca_cube = transform_cube_to_pca(cube, pca)
    residual = residual_map_for_image(model, cube, pca_cube, label_mask, pca, args, device)
    finite = np.isfinite(residual) & label_mask
    if not finite.any():
        raise RuntimeError(f"No finite residual pixels for {hdr_path.name}.")

    mean_residual = float(np.mean(residual[finite]))
    p95_residual = float(np.percentile(residual[finite], 95))
    hot_area_ratio = float(np.mean(residual[finite] > calibration_p99))
    normalized_mean_error = min(mean_residual / max(calibration_p99, 1e-12), 1.0)
    uniformity_score = 100.0 * (1.0 - normalized_mean_error)

    output_dir = output_root / (output_name or hdr_path.stem)
    rgb = pseudo_rgb(cube, wavelengths)
    save_residual_images(output_dir, residual, label_mask, rgb)
    save_result_summary(
        output_dir=output_dir,
        stem=hdr_path.stem,
        rgb=rgb,
        residual=residual,
        label_mask=label_mask,
        uniformity_score=uniformity_score,
        mean_residual=mean_residual,
        p95_residual=p95_residual,
        hot_area_ratio=hot_area_ratio,
    )
    np.save(output_dir / "residual_map.npy", np.nan_to_num(residual, nan=0.0).astype(np.float32))

    row = {
        "stem": hdr_path.stem,
        "mean_residual": mean_residual,
        "p95_residual": p95_residual,
        "calibration_pixel_p99": calibration_p99,
        "hot_area_ratio_gt_val_p99": hot_area_ratio,
        "normalized_mean_error": normalized_mean_error,
        "uniformity_score_0_100": uniformity_score,
        "residual_space": args.residual_space,
        "label_fraction_of_sample": mask_meta["label_fraction_of_sample"],
        "sam_threshold_deg": mask_meta["sam_threshold_deg"],
        "output_dir": str(output_dir),
    }
    print(f"[done] {hdr_path.stem}: uniformity={uniformity_score:.1f}% output={output_dir}")
    return row


def write_metrics(output_root: Path, rows: list[dict]) -> None:
    if not rows:
        return
    path = output_root / "inference_metrics.csv"
    with path.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)
    print(f"[metrics] {path}")


def resolve_run_dir(script_dir: Path, value: str | Path) -> Path:
    run_dir = Path(value)
    if not run_dir.is_absolute():
        run_dir = script_dir / run_dir
    return run_dir.resolve()


def load_inference_bundle(
    run_dir: Path,
    cli_args: argparse.Namespace,
    data_dir: Path,
    device,
) -> dict:
    run_args = load_run_args(run_dir, cli_args)
    run_args.include_excluded = cli_args.include_excluded
    pca = load_pca(run_dir)
    model = load_model(run_dir, device)
    calibration_p99 = cli_args.calibration_p99 or calibration_from_metrics(run_dir)
    if calibration_p99 is None:
        calibration_p99 = calibration_from_val_split(run_dir, data_dir, model, pca, run_args, device)
    print(f"[loaded] {run_dir} calibration_pixel_p99={calibration_p99:.6g}")
    return {
        "run_dir": run_dir,
        "run_args": run_args,
        "pca": pca,
        "model": model,
        "calibration_p99": calibration_p99,
    }


def resolve_auto_thickness(hdr_path: Path, cli_args: argparse.Namespace, classifier: dict | None) -> dict:
    parsed = parse_thickness_from_sample_id(hdr_path.stem)
    if cli_args.thickness_source == "filename":
        if parsed is None:
            raise ValueError(f"Cannot parse thickness from filename: {hdr_path.stem}")
        value, label = parsed
        return {
            "predicted_thickness": label,
            "predicted_thickness_um": value,
            "thickness_confidence": "",
            "thickness_source": "filename",
        }
    if cli_args.thickness_source == "auto" and parsed is not None:
        value, label = parsed
        return {
            "predicted_thickness": label,
            "predicted_thickness_um": value,
            "thickness_confidence": "",
            "thickness_source": "filename",
        }
    if classifier is None:
        raise ValueError("A thickness classifier is required. Run train_thickness_classifier.py first.")
    result = predict_thickness_for_hdr(hdr_path, cli_args, classifier=classifier)
    result["thickness_source"] = "classifier"
    return result


def confidence_is_low(thickness_info: dict, threshold: float) -> bool:
    try:
        confidence = float(thickness_info.get("thickness_confidence", ""))
    except (TypeError, ValueError):
        return False
    return confidence < threshold


def candidate_score(row: dict) -> tuple[float, float, float]:
    return (
        float(row.get("normalized_mean_error", float("inf"))),
        float(row.get("hot_area_ratio_gt_val_p99", float("inf"))),
        float(row.get("mean_residual", float("inf"))),
    )


def run_candidate(
    hdr_path: Path,
    output_root: Path,
    label: str,
    bundle_cache: dict[str, dict],
    run_dir_map: dict[str, Path],
    cli_args: argparse.Namespace,
    data_dir: Path,
    device,
    suffix: str | None = None,
) -> dict:
    if label not in bundle_cache:
        bundle_cache[label] = load_inference_bundle(run_dir_map[label], cli_args, data_dir, device)
    bundle = bundle_cache[label]
    output_name = f"{hdr_path.stem}_{suffix}" if suffix else None
    row = infer_one(
        hdr_path,
        output_root,
        bundle["model"],
        bundle["pca"],
        bundle["run_args"],
        device,
        bundle["calibration_p99"],
        output_name=output_name,
    )
    if row:
        row["cae_run_dir"] = str(run_dir_map[label])
        row["cae_model_label"] = label
    return row


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Run inference with a trained CAE model.")
    parser.add_argument("--run-dir", default=None, help="Example: cae_runs/cae_50u_v1. If omitted, --auto-thickness is used.")
    parser.add_argument("--auto-thickness", action="store_true", help="Predict thickness and route each image to the 50u/70u CAE model.")
    parser.add_argument("--run-dir-50u", default="cae_runs/cae_50u_v1")
    parser.add_argument("--run-dir-70u", default="cae_runs/cae_70u_v1")
    parser.add_argument("--run-dir-all", default="cae_runs/cae_all_v1")
    parser.add_argument("--thickness-model", default=str(DEFAULT_THICKNESS_MODEL))
    parser.add_argument("--thickness-source", choices=["auto", "filename", "predict"], default="predict")
    parser.add_argument("--min-thickness-confidence", type=float, default=0.75)
    parser.add_argument(
        "--low-confidence-policy",
        choices=["dual-cae", "all-model", "use-classifier"],
        default="dual-cae",
        help="Policy when predicted thickness confidence is below threshold.",
    )
    parser.add_argument("--data-dir", default=None, help="Folder containing *_RT.hdr/raw files.")
    parser.add_argument("--file", default=None, help="Single HDR filename or absolute path.")
    parser.add_argument("--all", action="store_true", help="Run every matching HDR file.")
    parser.add_argument("--thickness", choices=["50u", "70u", "all"], default=None)
    parser.add_argument("--include-excluded", action="store_true")
    parser.add_argument("--output-dir", default=None)
    parser.add_argument("--device", default=None)
    parser.add_argument("--residual-space", choices=["spectral", "pca"], default=None)
    parser.add_argument("--calibration-p99", type=float, default=None)
    parser.add_argument("--trim-bands", type=int, default=10)
    parser.add_argument("--label-polarity", choices=["auto", "bright", "dark"], default="auto")
    parser.add_argument("--seed-percentile", type=float, default=85.0)
    parser.add_argument("--sam-threshold-deg", type=float, default=None)
    parser.add_argument("--max-label-fraction", type=float, default=0.32)
    return parser


def main() -> None:
    cli_args = build_parser().parse_args()
    script_dir = Path(__file__).resolve().parent
    auto_thickness = cli_args.auto_thickness or cli_args.run_dir is None
    if not auto_thickness and cli_args.run_dir is None:
        raise ValueError("Pass --run-dir or use --auto-thickness.")

    if auto_thickness:
        data_dir = resolve_data_dir(script_dir, cli_args.data_dir)
    else:
        run_dir = resolve_run_dir(script_dir, cli_args.run_dir)
        run_args = load_run_args(run_dir, cli_args)
        data_dir = resolve_data_dir(script_dir, cli_args.data_dir or getattr(run_args, "data_dir", None))

    device = torch.device(cli_args.device or ("cuda" if torch.cuda.is_available() else "cpu"))
    print(f"[data] {data_dir}")
    print(f"[device] {device}")

    if auto_thickness:
        run_dir_map = {
            "50u": resolve_run_dir(script_dir, cli_args.run_dir_50u),
            "70u": resolve_run_dir(script_dir, cli_args.run_dir_70u),
            "all": resolve_run_dir(script_dir, cli_args.run_dir_all),
        }
        classifier = None
        if cli_args.thickness_source in {"auto", "predict"}:
            classifier_path = Path(cli_args.thickness_model)
            classifier = load_thickness_classifier(classifier_path)
            print(f"[thickness-model] {classifier_path}")
        output_root = Path(cli_args.output_dir) if cli_args.output_dir else script_dir / "cae_runs" / "auto_thickness_inference_outputs"
        input_paths = choose_inputs(data_dir, cli_args, None)
        bundle_cache: dict[str, dict] = {}
    else:
        run_dir = resolve_run_dir(script_dir, cli_args.run_dir)
        print(f"[run] {run_dir}")
        bundle = load_inference_bundle(run_dir, cli_args, data_dir, device)
        output_root = Path(cli_args.output_dir) if cli_args.output_dir else run_dir / "inference_outputs"
        input_paths = choose_inputs(data_dir, cli_args, bundle["run_args"])

    if not output_root.is_absolute():
        output_root = script_dir / output_root
    output_root.mkdir(parents=True, exist_ok=True)

    rows = []
    for hdr_path in input_paths:
        if auto_thickness:
            thickness_info = resolve_auto_thickness(hdr_path, cli_args, classifier)
            label = thickness_info["predicted_thickness"]
            low_confidence = confidence_is_low(thickness_info, cli_args.min_thickness_confidence)
            selection_policy = "classifier"
            candidate_rows: dict[str, dict] = {}

            if low_confidence and cli_args.low_confidence_policy == "dual-cae":
                selection_policy = "dual-cae-low-confidence"
                for candidate_label in ["50u", "70u"]:
                    candidate_rows[candidate_label] = run_candidate(
                        hdr_path,
                        output_root,
                        candidate_label,
                        bundle_cache,
                        run_dir_map,
                        cli_args,
                        data_dir,
                        device,
                        suffix=f"{candidate_label}_candidate",
                    )
                usable = {key: value for key, value in candidate_rows.items() if value}
                if not usable:
                    row = {}
                else:
                    label = min(usable, key=lambda key: candidate_score(usable[key]))
                    row = usable[label]
            elif low_confidence and cli_args.low_confidence_policy == "all-model":
                selection_policy = "all-model-low-confidence"
                label = "all"
                row = run_candidate(hdr_path, output_root, label, bundle_cache, run_dir_map, cli_args, data_dir, device)
            else:
                row = run_candidate(hdr_path, output_root, label, bundle_cache, run_dir_map, cli_args, data_dir, device)

            if row:
                row.update(
                    {
                        "predicted_thickness": thickness_info["predicted_thickness"],
                        "predicted_thickness_um": thickness_info["predicted_thickness_um"],
                        "classifier_predicted_thickness": thickness_info["predicted_thickness"],
                        "selected_cae_model_label": label,
                        "thickness_confidence": thickness_info.get("thickness_confidence", ""),
                        "thickness_source": thickness_info["thickness_source"],
                        "thickness_low_confidence": low_confidence,
                        "min_thickness_confidence": cli_args.min_thickness_confidence,
                        "model_selection_policy": selection_policy,
                        "candidate_50u_normalized_mean_error": candidate_rows.get("50u", {}).get("normalized_mean_error", ""),
                        "candidate_70u_normalized_mean_error": candidate_rows.get("70u", {}).get("normalized_mean_error", ""),
                    }
                )
        else:
            row = infer_one(
                hdr_path,
                output_root,
                bundle["model"],
                bundle["pca"],
                bundle["run_args"],
                device,
                bundle["calibration_p99"],
            )
            if row:
                row["cae_run_dir"] = str(run_dir)
        if row:
            rows.append(row)
    write_metrics(output_root, rows)


if __name__ == "__main__":
    main()
