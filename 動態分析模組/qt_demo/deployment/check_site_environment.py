from __future__ import annotations

import argparse
import csv
import hashlib
import importlib.metadata
import json
import platform
import struct
import sys
import traceback
from pathlib import Path


SCRIPT_DIR = Path(__file__).resolve().parent
QT_DIR = SCRIPT_DIR.parent
BUNDLE_ROOT = QT_DIR.parents[1]

REQUIRED_FILES = (
    "動態分析模組/qt_demo/app.py",
    "動態分析模組/qt_demo/settings.example.json",
    "動態分析模組/fx_test/mvGenTLProducer.cti",
    "動態分析模組/calibration/FX17_076900034157_3ms_20260728_023239_Ref-Dark.dat",
    "動態分析模組/calibration/FX17_076900034157_3ms_20260728_023239_Ref-White.dat",
    "20260528_導電材料/50u_1.hdr",
    "CAE訓練/auto_sam_mask.py",
    "CAE訓練/infer_cae.py",
    "CAE訓練/train_cae.py",
    "CAE訓練/cae_runs/cae_site16_ft_4fold_oof_50ep/final_model/best_cae.pt",
    "CAE訓練/cae_runs/cae_site16_ft_4fold_oof_50ep/final_model/config.json",
    "CAE訓練/cae_runs/cae_site16_ft_4fold_oof_50ep/final_model/pca_model.npz",
    "CAE訓練/cae_runs/cae_site16_ft_4fold_oof_50ep/final_model/site16_oof_calibration.json",
    "電阻值模型/predict_resistance.py",
    "電阻值模型/train_resistance_model.py",
    "電阻值模型/thickness_classifier.py",
    "電阻值模型/resistance_model_no_thickness_v3_outputs/resistance_model_no_thickness.npz",
)

PACKAGES = (
    "numpy",
    "PySide6",
    "opencv-python",
    "opencv-python-headless",
    "harvesters",
    "genicam",
    "torch",
    "openpyxl",
)


def verify_bundle_manifest() -> dict:
    manifest_path = BUNDLE_ROOT / "BUNDLE_SHA256.csv"
    if not manifest_path.is_file():
        return {"ok": False, "error": "BUNDLE_SHA256.csv is missing"}
    missing: list[str] = []
    mismatched: list[str] = []
    checked = 0
    with manifest_path.open("r", encoding="utf-8-sig", newline="") as stream:
        for row in csv.DictReader(stream):
            relative = row["RelativePath"]
            path = BUNDLE_ROOT / Path(relative)
            if not path.is_file():
                missing.append(relative)
                continue
            expected_size = int(row["Length"])
            digest = hashlib.sha256()
            with path.open("rb") as source:
                for chunk in iter(lambda: source.read(1024 * 1024), b""):
                    digest.update(chunk)
            if path.stat().st_size != expected_size or digest.hexdigest().casefold() != row[
                "SHA256"
            ].casefold():
                mismatched.append(relative)
            checked += 1
    return {
        "ok": not missing and not mismatched,
        "checked": checked,
        "missing": missing,
        "mismatched": mismatched,
    }


def package_version(name: str) -> str | None:
    try:
        return importlib.metadata.version(name)
    except importlib.metadata.PackageNotFoundError:
        return None


def build_report(*, load_models: bool) -> dict:
    missing_files = [
        relative
        for relative in REQUIRED_FILES
        if not (BUNDLE_ROOT / Path(relative)).is_file()
    ]
    report = {
        "bundle_root": str(BUNDLE_ROOT),
        "python": sys.version,
        "python_executable": sys.executable,
        "python_64bit": struct.calcsize("P") * 8 == 64,
        "platform": platform.platform(),
        "packages": {name: package_version(name) for name in PACKAGES},
        "bundle_manifest": verify_bundle_manifest(),
        "missing_files": missing_files,
        "cuda": None,
        "model_load": None,
    }
    try:
        import torch

        report["cuda"] = {
            "torch_version": torch.__version__,
            "torch_cuda_build": torch.version.cuda,
            "available": torch.cuda.is_available(),
            "device": (
                torch.cuda.get_device_name(0)
                if torch.cuda.is_available()
                else None
            ),
        }
    except Exception:
        report["cuda"] = {"error": traceback.format_exc()}

    if load_models and not missing_files:
        sys.path.insert(0, str(QT_DIR))
        try:
            from live_analysis import LiveModelAnalyzer

            device = (
                "cuda"
                if report["cuda"] and report["cuda"].get("available")
                else "cpu"
            )
            analyzer = LiveModelAnalyzer(device=device)
            report["model_load"] = {
                "ok": True,
                "device": str(analyzer.device),
                "cae_run_dir": str(analyzer.cae_run_dir),
                "resistance_model": str(analyzer.resistance_model_path),
            }
        except Exception:
            report["model_load"] = {
                "ok": False,
                "error": traceback.format_exc(),
            }
    return report


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Check the copied FX17 site bundle without touching the camera."
    )
    parser.add_argument(
        "--load-models",
        action="store_true",
        help="Also load the CAE and resistance models on CUDA or CPU.",
    )
    parser.add_argument("--report", help="Optional JSON output path.")
    args = parser.parse_args()
    report = build_report(load_models=args.load_models)
    text = json.dumps(report, ensure_ascii=False, indent=2)
    print(text)
    if args.report:
        Path(args.report).resolve().write_text(text + "\n", encoding="utf-8")
    packages_ok = all(
        report["packages"][name] is not None
        for name in ("numpy", "PySide6", "harvesters", "genicam", "torch")
    )
    models_ok = not args.load_models or bool(
        report["model_load"] and report["model_load"].get("ok")
    )
    manifest_ok = bool(report["bundle_manifest"].get("ok"))
    return (
        0
        if not report["missing_files"] and packages_ok and models_ok and manifest_ok
        else 1
    )


if __name__ == "__main__":
    raise SystemExit(main())
