from __future__ import annotations

import argparse
import math
import sys
from pathlib import Path

import numpy as np


SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

from train_resistance_model import Sample, extract_one  # noqa: E402


DEFAULT_THICKNESS_MODEL = SCRIPT_DIR / "resistance_model_outputs" / "thickness_classifier.npz"


def format_thickness_label(value: float | int | str) -> str:
    if isinstance(value, str):
        text = value.strip()
        if text.endswith("u"):
            return text
        return f"{float(text):.0f}u"
    return f"{float(value):.0f}u"


def parse_thickness_from_sample_id(sample_id: str) -> tuple[float, str] | None:
    first = sample_id.split("_", 1)[0].strip().lower()
    if not first.endswith("u"):
        return None
    try:
        value = float(first[:-1])
    except ValueError:
        return None
    return value, format_thickness_label(value)


def scalar_value(array) -> float | str:
    value = array.item() if hasattr(array, "shape") and array.shape == () else array
    if isinstance(value, bytes):
        return value.decode("utf-8")
    return value


def load_thickness_classifier(path: Path | str = DEFAULT_THICKNESS_MODEL) -> dict:
    model_path = Path(path)
    if not model_path.exists():
        raise FileNotFoundError(
            f"Missing thickness classifier: {model_path}. "
            "Run train_thickness_classifier.py first."
        )
    data = np.load(model_path, allow_pickle=True)
    model = {key: data[key] for key in data.files}
    model["model_path"] = np.array(str(model_path))
    return model


def feature_vector_from_scalar_spectrum(scalar: dict, spectrum: np.ndarray, feature_names: list[str]) -> np.ndarray:
    values = []
    for name in feature_names:
        if name.startswith("spec_"):
            values.append(float(spectrum[int(name.split("_")[1])]))
        else:
            values.append(float(scalar[name]))
    return np.array(values, dtype=np.float64).reshape(1, -1)


def predict_thickness_from_vector(classifier: dict, X: np.ndarray) -> list[dict]:
    kind = str(scalar_value(classifier["kind"]))
    x_mean = classifier["x_mean"].astype(np.float64)
    x_std = classifier["x_std"].astype(np.float64)
    classes = [str(x) for x in classifier["classes"].tolist()]

    results = []
    if kind == "pls_regression_classifier":
        xs = (X - x_mean) / x_std
        predicted_value = xs @ classifier["coef_scaled"].astype(np.float64)
        predicted_value = predicted_value * float(scalar_value(classifier["y_std"])) + float(scalar_value(classifier["y_mean"]))
        threshold = float(scalar_value(classifier["decision_threshold"]))
        confidence_scale = max(float(scalar_value(classifier["confidence_scale"])), 1e-6)
        p70 = 1.0 / (1.0 + np.exp(-(predicted_value - threshold) / confidence_scale))
        for value, prob70 in zip(predicted_value.ravel(), p70.ravel()):
            label = "70u" if value >= threshold else "50u"
            confidence = prob70 if label == "70u" else 1.0 - prob70
            results.append(
                {
                    "predicted_thickness": label,
                    "predicted_thickness_um": float(label.replace("u", "")),
                    "predicted_thickness_raw": float(value),
                    "thickness_confidence": float(confidence),
                    "distance_to_50u": float(abs(value - 50.0)),
                    "distance_to_70u": float(abs(value - 70.0)),
                }
            )
        return results

    if kind == "pca_nearest_centroid":
        components = classifier["components"].astype(np.float64)
        centroids = classifier["centroids"].astype(np.float64)
        confidence_scale = float(scalar_value(classifier["confidence_scale"]))
        confidence_scale = max(confidence_scale, 1e-6)

        scores = ((X - x_mean) / x_std) @ components
        distances = np.linalg.norm(scores[:, None, :] - centroids[None, :, :], axis=2)
        logits = -distances / confidence_scale
        logits = logits - logits.max(axis=1, keepdims=True)
        probs = np.exp(logits)
        probs = probs / probs.sum(axis=1, keepdims=True)
        indices = np.argmin(distances, axis=1)

        for row_idx, class_idx in enumerate(indices):
            label = classes[int(class_idx)]
            results.append(
                {
                    "predicted_thickness": label,
                    "predicted_thickness_um": float(label.replace("u", "")),
                    "predicted_thickness_raw": "",
                    "thickness_confidence": float(probs[row_idx, int(class_idx)]),
                    "distance_to_50u": float(distances[row_idx, classes.index("50u")]) if "50u" in classes else math.nan,
                    "distance_to_70u": float(distances[row_idx, classes.index("70u")]) if "70u" in classes else math.nan,
                }
            )
        return results

    raise ValueError(f"Unsupported thickness classifier kind: {kind}")


def predict_thickness_from_scalar_spectrum(classifier: dict, scalar: dict, spectrum: np.ndarray) -> dict:
    feature_names = [str(x) for x in classifier["feature_names"].tolist()]
    X = feature_vector_from_scalar_spectrum(scalar, spectrum, feature_names)
    return predict_thickness_from_vector(classifier, X)[0]


def predict_thickness_for_hdr(
    hdr_path: Path,
    args: argparse.Namespace,
    classifier: dict | None = None,
    model_path: Path | str = DEFAULT_THICKNESS_MODEL,
) -> dict:
    classifier = classifier or load_thickness_classifier(model_path)
    sample_id = hdr_path.stem
    sample = Sample(sample_id=sample_id, thickness_um=0.0, resistance_ohm=0.0, hdr_path=hdr_path)
    scalar, spectrum, row = extract_one(sample, args)
    result = predict_thickness_from_scalar_spectrum(classifier, scalar, spectrum)
    result.update(
        {
            "sample_id": sample_id,
            "model_path": str(scalar_value(classifier["model_path"])),
            "sam_threshold_deg": row.get("sam_threshold_deg"),
            "label_fraction_of_sample": row.get("label_fraction_of_sample"),
        }
    )
    return result
