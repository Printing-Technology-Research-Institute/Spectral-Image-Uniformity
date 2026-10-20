from __future__ import annotations

import importlib.util
import sys
from pathlib import Path


HERE = Path(__file__).resolve().parent
SOURCE_SCRIPT = HERE.parent / "fx17_site_finetune_4fold" / "generate_oof_report_figures.py"


def load_source_module():
    spec = importlib.util.spec_from_file_location("fx17_oof_plot_base", SOURCE_SCRIPT)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"Cannot load plot script: {SOURCE_SCRIPT}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


plot = load_source_module()
plot.HERE = HERE
plot.OUTPUT_DIR = HERE / "oof_report_figures"
plot.INDIVIDUAL_DIR = plot.OUTPUT_DIR / "individual"
plot.CHALLENGE_DIR = plot.OUTPUT_DIR / "challenge_holdout"
plot.FINAL_MODEL_DIR = HERE / "final_model"
plot.METRICS_PATH = HERE / "oof_metrics.csv"
plot.CALIBRATION_PATH = HERE / "site16_oof_calibration.json"
plot.OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
plot.INDIVIDUAL_DIR.mkdir(parents=True, exist_ok=True)
plot.CHALLENGE_DIR.mkdir(parents=True, exist_ok=True)


if __name__ == "__main__":
    plot.main()
