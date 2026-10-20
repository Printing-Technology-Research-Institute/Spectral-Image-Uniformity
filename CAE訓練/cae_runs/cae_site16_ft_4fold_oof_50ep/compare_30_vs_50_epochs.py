from __future__ import annotations

import csv
import json
import os
import sys
from pathlib import Path


HERE = Path(__file__).resolve().parent
RUN30 = HERE.parent / "fx17_site_finetune_4fold"
OUT = HERE / "oof_report_figures"
MPL_DEPS = HERE.parent / "fx17_uniformity_diagnostic" / "matplotlib_deps"
MPL_CONFIG = HERE / "matplotlib_config"
MPL_CONFIG.mkdir(parents=True, exist_ok=True)
os.environ.setdefault("MPLCONFIGDIR", str(MPL_CONFIG))
if MPL_DEPS.exists():
    sys.path.insert(0, str(MPL_DEPS))

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib import font_manager
import numpy as np


FONT_PATH = Path(r"C:\Windows\Fonts\msjh.ttc")
if FONT_PATH.exists():
    font_manager.fontManager.addfont(str(FONT_PATH))
    FONT_NAME = font_manager.FontProperties(fname=str(FONT_PATH)).get_name()
else:
    FONT_NAME = "DejaVu Sans"

BLUE = "#2F6B9A"
ORANGE = "#D97732"
INK = "#1F2937"
MUTED = "#667085"
GRID = "#D9DEE5"

plt.rcParams.update(
    {
        "font.family": FONT_NAME,
        "axes.unicode_minus": False,
        "figure.facecolor": "white",
        "axes.facecolor": "white",
        "savefig.facecolor": "white",
        "axes.edgecolor": "#AAB2BD",
        "text.color": INK,
        "xtick.color": INK,
        "ytick.color": INK,
    }
)


def read_csv(path: Path) -> dict[str, dict[str, str]]:
    with path.open(newline="", encoding="utf-8-sig") as handle:
        return {row["sample_id"]: row for row in csv.DictReader(handle)}


def read_fold_history(fold: int) -> tuple[np.ndarray, np.ndarray]:
    path = HERE / "folds" / f"fold_{fold}" / "history.csv"
    with path.open(newline="", encoding="utf-8-sig") as handle:
        rows = list(csv.DictReader(handle))
    return (
        np.array([int(row["epoch"]) for row in rows], dtype=int),
        np.array([float(row["val_loss"]) for row in rows], dtype=float),
    )


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    rows30 = read_csv(RUN30 / "oof_metrics.csv")
    rows50 = read_csv(HERE / "oof_metrics.csv")
    sample_ids = sorted(rows30)
    if sample_ids != sorted(rows50) or len(sample_ids) != 16:
        raise RuntimeError("30/50 epoch OOF sample sets do not match")

    calibration30 = json.loads(
        (RUN30 / "site16_oof_calibration.json").read_text(encoding="utf-8")
    )
    calibration50 = json.loads(
        (HERE / "site16_oof_calibration.json").read_text(encoding="utf-8")
    )
    p99_30 = float(calibration30["calibration_pixel_p99"])
    p99_50 = float(calibration50["calibration_pixel_p99"])
    residual30 = np.array(
        [float(rows30[s]["tuned_mean_residual"]) for s in sample_ids]
    )
    residual50 = np.array(
        [float(rows50[s]["tuned_mean_residual"]) for s in sample_ids]
    )
    uniformity30_common = np.clip(100.0 * (1.0 - residual30 / p99_30), 0, 100)
    uniformity50_common = np.clip(100.0 * (1.0 - residual50 / p99_30), 0, 100)
    paired_change = 100.0 * (residual50 - residual30) / residual30

    histories = [read_fold_history(fold)[1] for fold in range(4)]
    mean_val = np.mean(np.vstack(histories), axis=0)
    epochs = np.arange(1, len(mean_val) + 1)

    fig = plt.figure(figsize=(14.5, 9.4))
    grid = fig.add_gridspec(2, 2, height_ratios=[1.05, 1.0], hspace=0.36, wspace=0.22)
    ax1 = fig.add_subplot(grid[0, 0])
    ax2 = fig.add_subplot(grid[0, 1])
    ax3 = fig.add_subplot(grid[1, :])

    x = np.arange(len(sample_ids))
    width = 0.38
    labels = [s.replace("FX17_", "") for s in sample_ids]
    ax1.bar(x - width / 2, residual30 * 1e5, width, label="30 epochs", color=BLUE)
    ax1.bar(x + width / 2, residual50 * 1e5, width, label="50 epochs", color=ORANGE)
    ax1.set_title("16 張樣本 OOF 平均殘差（越低越好）", fontweight="bold")
    ax1.set_ylabel("平均殘差 × 1e5")
    ax1.set_xticks(x, labels, rotation=55, ha="right")
    ax1.grid(axis="y", color=GRID, linewidth=0.8, alpha=0.8)
    ax1.set_axisbelow(True)
    ax1.legend(frameon=False, ncol=2)

    ax2.bar(x - width / 2, uniformity30_common, width, label="30 epochs", color=BLUE)
    ax2.bar(x + width / 2, uniformity50_common, width, label="50 epochs", color=ORANGE)
    ax2.set_title("固定 30-epoch OOF p99 的均勻度比較", fontweight="bold")
    ax2.set_ylabel("均勻度 (%)")
    ax2.set_xticks(x, labels, rotation=55, ha="right")
    ax2.grid(axis="y", color=GRID, linewidth=0.8, alpha=0.8)
    ax2.set_axisbelow(True)
    ax2.legend(frameon=False, ncol=2)

    for fold, values in enumerate(histories):
        ax3.plot(epochs, values, linewidth=1.2, alpha=0.35, label=f"Fold {fold}")
    ax3.plot(epochs, mean_val, color=INK, linewidth=2.6, label="4-fold 平均")
    ax3.axvline(30, color=ORANGE, linestyle="--", linewidth=1.6)
    ax3.text(30.6, max(mean_val) * 0.96, "epoch 31 起 LR 2e-5", color=ORANGE, va="top")
    ax3.scatter([30, 50], [mean_val[29], mean_val[49]], color=[BLUE, ORANGE], zorder=5)
    ax3.set_title("50-epoch 實驗的 validation loss 收斂曲線", fontweight="bold")
    ax3.set_xlabel("Epoch")
    ax3.set_ylabel("Validation loss")
    ax3.grid(color=GRID, linewidth=0.8, alpha=0.8)
    ax3.set_axisbelow(True)
    ax3.legend(frameon=False, ncol=5, fontsize=9, loc="upper right")

    mean30 = float(residual30.mean())
    mean50 = float(residual50.mean())
    mean_change = 100.0 * (mean50 - mean30) / mean30
    fig.suptitle("FX17 fine-tune：30 vs 50 epochs（相同 4-fold OOF）", x=0.055, y=0.985, ha="left", fontsize=20, fontweight="bold")
    fig.text(
        0.055,
        0.945,
        f"OOF 平均殘差 {mean30:.4g} → {mean50:.4g}（{mean_change:.2f}%）；"
        f"p99 {p99_30:.4g} → {p99_50:.4g}。右圖固定同一門檻，避免校正基準改變造成誤讀。",
        fontsize=11.2,
        color=MUTED,
    )
    fig.savefig(OUT / "epochs_30_vs_50_oof_comparison.png", dpi=300, bbox_inches="tight")
    plt.close(fig)

    comparison_path = OUT / "epochs_30_vs_50_per_sample.csv"
    with comparison_path.open("w", newline="", encoding="utf-8-sig") as handle:
        writer = csv.writer(handle)
        writer.writerow(
            [
                "sample_id",
                "oof_mean_residual_30",
                "oof_mean_residual_50",
                "residual_change_percent_50_vs_30",
                "uniformity_30_fixed_p99_30",
                "uniformity_50_fixed_p99_30",
            ]
        )
        for i, sample_id in enumerate(sample_ids):
            writer.writerow(
                [
                    sample_id,
                    residual30[i],
                    residual50[i],
                    paired_change[i],
                    uniformity30_common[i],
                    uniformity50_common[i],
                ]
            )

    summary = {
        "mean_oof_residual_30": mean30,
        "mean_oof_residual_50": mean50,
        "mean_oof_residual_change_percent_50_vs_30": mean_change,
        "oof_p99_30": p99_30,
        "oof_p99_50": p99_50,
        "oof_p99_change_percent_50_vs_30": 100.0 * (p99_50 - p99_30) / p99_30,
        "mean_uniformity_30_fixed_p99_30": float(uniformity30_common.mean()),
        "mean_uniformity_50_fixed_p99_30": float(uniformity50_common.mean()),
        "mean_val_loss_epoch_30": float(mean_val[29]),
        "mean_val_loss_epoch_50": float(mean_val[49]),
        "mean_val_loss_change_percent_50_vs_30": float(
            100.0 * (mean_val[49] - mean_val[29]) / mean_val[29]
        ),
        "best_epochs_by_fold": calibration50["best_epochs_by_fold"],
        "samples_improved": int(np.sum(residual50 < residual30)),
        "sample_count": len(sample_ids),
    }
    (OUT / "epochs_30_vs_50_summary.json").write_text(
        json.dumps(summary, ensure_ascii=False, indent=2), encoding="utf-8"
    )
    print(json.dumps(summary, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
