from __future__ import annotations

import importlib.util
import sys
from pathlib import Path


HERE = Path(__file__).resolve().parent
SOURCE_ROOT = HERE.parent / "fx17_site_finetune_4fold"
SOURCE_SCRIPT = SOURCE_ROOT / "finetune_site16_oof.py"


def load_source_module():
    spec = importlib.util.spec_from_file_location("fx17_site16_oof_base", SOURCE_SCRIPT)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"Cannot load source script: {SOURCE_SCRIPT}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


ft = load_source_module()

# Keep the exact same prepared pixels and sample-level fold assignment as the
# 30-epoch experiment. Only the optimization horizon/schedule changes.
ft.ROOT = HERE
ft.PREPARED_DIR = SOURCE_ROOT / "prepared_site16"
ft.FOLDS_DIR = HERE / "folds"
ft.OOF_DIR = HERE / "oof"
ft.FINAL_DIR = HERE / "final_model"
ft.CALIBRATION_PATH = HERE / "site16_oof_calibration.json"
ft.ASSIGNMENTS_PATH = HERE / "fold_assignments.json"
ft.EPOCHS = 50
ft.PATIENCE = 8
ft.LEARNING_RATE = 1e-4
ft.LR_DROP_AFTER_EPOCH = 30
ft.LR_AFTER_DROP = 2e-5


if __name__ == "__main__":
    ft.main()
