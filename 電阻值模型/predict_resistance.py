from __future__ import annotations

import argparse
import csv
import json
import sys
from pathlib import Path

import numpy as np


SCRIPT_DIR = Path(__file__).resolve().parent
WORKSPACE_DIR = SCRIPT_DIR.parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

from train_resistance_model import (  # noqa: E402
    Sample,
    extract_one,
    parse_sample_number,
    resolve_data_dir,
)
from thickness_classifier import (  # noqa: E402
    DEFAULT_THICKNESS_MODEL,
    format_thickness_label,
    load_thickness_classifier,
    parse_thickness_from_sample_id,
    predict_thickness_from_scalar_spectrum,
)


def load_model(model_path: Path) -> tuple[dict, dict]:
    data = np.load(model_path, allow_pickle=True)
    model = {key: data[key] for key in data.files}
    metadata_path = model_path.with_suffix(".json")
    metadata = json.loads(metadata_path.read_text(encoding="utf-8")) if metadata_path.exists() else {}
    return model, metadata


def scalar_value(array) -> float | str:
    value = array.item() if hasattr(array, "shape") and array.shape == () else array
    if isinstance(value, bytes):
        return value.decode("utf-8")
    return value


def resolve_thickness(
    sample_id: str,
    scalar: dict,
    spectrum: np.ndarray,
    classifier: dict | None,
    args: argparse.Namespace,
) -> dict:
    parsed = parse_thickness_from_sample_id(sample_id)
    if args.thickness_source == "manual":
        if not args.manual_thickness:
            raise ValueError("--manual-thickness is required when --thickness-source manual.")
        label = format_thickness_label(args.manual_thickness)
        return {
            "thickness_um": float(label.replace("u", "")),
            "thickness_label": label,
            "thickness_source": "manual",
            "thickness_confidence": "",
        }

    if args.thickness_source == "filename":
        if parsed is None:
            raise ValueError(f"Cannot parse thickness from filename: {sample_id}")
        value, label = parsed
        return {
            "thickness_um": value,
            "thickness_label": label,
            "thickness_source": "filename",
            "thickness_confidence": "",
        }

    if args.thickness_source == "auto" and parsed is not None:
        value, label = parsed
        return {
            "thickness_um": value,
            "thickness_label": label,
            "thickness_source": "filename",
            "thickness_confidence": "",
        }

    if classifier is None:
        raise ValueError("A thickness classifier is required. Run train_thickness_classifier.py first.")
    prediction = predict_thickness_from_scalar_spectrum(classifier, scalar, spectrum)
    confidence = prediction["thickness_confidence"]
    return {
        "thickness_um": float(prediction["predicted_thickness_um"]),
        "thickness_label": prediction["predicted_thickness"],
        "thickness_source": "classifier",
        "thickness_confidence": confidence,
        "predicted_thickness_raw": prediction.get("predicted_thickness_raw", ""),
        "thickness_low_confidence": confidence < args.min_thickness_confidence,
        "distance_to_50u": prediction["distance_to_50u"],
        "distance_to_70u": prediction["distance_to_70u"],
    }


def feature_vector(
    sample_id: str,
    hdr_path: Path,
    model: dict,
    args: argparse.Namespace,
    classifier: dict | None,
) -> tuple[np.ndarray, dict]:
    sample = Sample(
        sample_id=sample_id,
        thickness_um=0.0,
        resistance_ohm=0.0,
        hdr_path=hdr_path,
    )
    scalar, spectrum, _ = extract_one(sample, args)
    thickness_info = resolve_thickness(sample_id, scalar, spectrum, classifier, args)
    scalar["thickness_um"] = thickness_info["thickness_um"]

    feature_names = [str(x) for x in model["feature_names"].tolist()]
    values = []
    for name in feature_names:
        if name.startswith("spec_"):
            values.append(float(spectrum[int(name.split("_")[1])]))
        else:
            values.append(float(scalar[name]))
    return np.array(values, dtype=np.float64).reshape(1, -1), thickness_info


def predict(model: dict, X: np.ndarray) -> np.ndarray:
    kind = str(scalar_value(model["kind"]))
    x_mean = model["x_mean"].astype(np.float64)
    x_std = model["x_std"].astype(np.float64)
    xs = (X - x_mean) / x_std
    if kind == "ridge":
        beta = model["beta"].astype(np.float64)
        return xs @ beta + float(scalar_value(model["y_mean"]))
    if kind == "pls":
        coef_scaled = model["coef_scaled"].astype(np.float64)
        return (xs @ coef_scaled) * float(scalar_value(model["y_std"])) + float(scalar_value(model["y_mean"]))
    raise ValueError(f"Unsupported model kind: {kind}")


def choose_files(data_dir: Path, args: argparse.Namespace) -> list[Path]:
    if args.file:
        path = Path(args.file)
        if not path.is_absolute():
            path = data_dir / path
        return [path]
    if args.all:
        headers = sorted(data_dir.glob("*_RT.hdr"))
        if args.thickness != "all":
            headers = [p for p in headers if p.stem.startswith(args.thickness + "_")]
        return headers
    raise ValueError("Pass --file <sample.hdr> or --all.")


def write_csv(path: Path, rows: list[dict]) -> None:
    if not rows:
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="", encoding="utf-8-sig") as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)


def build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Predict resistance from a trained resistance model.")
    parser.add_argument("--model", default=str(SCRIPT_DIR / "resistance_model_outputs" / "resistance_model.npz"))
    parser.add_argument("--thickness-model", default=str(DEFAULT_THICKNESS_MODEL))
    parser.add_argument("--data-dir", default=None)
    parser.add_argument("--file", default=None)
    parser.add_argument("--all", action="store_true")
    parser.add_argument("--thickness", choices=["50u", "70u", "all"], default="all")
    parser.add_argument("--thickness-source", choices=["auto", "filename", "predict", "manual"], default="predict")
    parser.add_argument("--manual-thickness", choices=["50u", "70u"], default=None)
    parser.add_argument("--min-thickness-confidence", type=float, default=0.75)
    parser.add_argument("--output", default=str(SCRIPT_DIR / "resistance_model_outputs" / "resistance_predictions.csv"))
    parser.add_argument("--trim-bands", type=int, default=10)
    parser.add_argument("--label-polarity", choices=["auto", "bright", "dark"], default="auto")
    parser.add_argument("--seed-percentile", type=float, default=85.0)
    parser.add_argument("--sam-threshold-deg", type=float, default=None)
    parser.add_argument("--max-label-fraction", type=float, default=0.32)
    return parser


def main() -> None:
    args = build_arg_parser().parse_args()
    model_path = Path(args.model)
    model, metadata = load_model(model_path)
    data_dir = resolve_data_dir(args.data_dir)
    classifier = None
    if args.thickness_source == "predict":
        classifier = load_thickness_classifier(args.thickness_model)
    elif args.thickness_source == "auto":
        classifier_path = Path(args.thickness_model)
        if classifier_path.exists():
            classifier = load_thickness_classifier(classifier_path)

    rows = []
    for hdr_path in choose_files(data_dir, args):
        sample_id = hdr_path.stem
        X, thickness_info = feature_vector(sample_id, hdr_path, model, args, classifier)
        pred = float(predict(model, X)[0])
        row = {
            "sample_id": sample_id,
            "sample_number": parse_sample_number(sample_id),
            "thickness_um": thickness_info["thickness_um"],
            "thickness_label": thickness_info["thickness_label"],
            "thickness_source": thickness_info["thickness_source"],
            "thickness_confidence": thickness_info.get("thickness_confidence", ""),
            "predicted_thickness_raw": thickness_info.get("predicted_thickness_raw", ""),
            "thickness_low_confidence": thickness_info.get("thickness_low_confidence", ""),
            "min_thickness_confidence": args.min_thickness_confidence,
            "distance_to_50u": thickness_info.get("distance_to_50u", ""),
            "distance_to_70u": thickness_info.get("distance_to_70u", ""),
            "predicted_resistance": pred,
            "model": str(scalar_value(model["kind"])),
            "feature_set": str(scalar_value(model["feature_set"])),
            "model_path": str(model_path),
        }
        rows.append(row)
        print(
            f"{sample_id}: thickness={thickness_info['thickness_label']} "
            f"source={thickness_info['thickness_source']} predicted resistance={pred:.4f}"
        )
    write_csv(Path(args.output), rows)
    print(f"[output] {args.output}")


if __name__ == "__main__":
    main()
