from __future__ import annotations

import csv
import json
import os
import sys
from pathlib import Path


HERE = Path(__file__).resolve().parent
MPL_DEPS = HERE.parent / "fx17_uniformity_diagnostic" / "matplotlib_deps"
MPL_CONFIG = HERE / "matplotlib_config"
OUTPUT_DIR = HERE / "oof_report_figures"
INDIVIDUAL_DIR = OUTPUT_DIR / "individual"
CHALLENGE_DIR = OUTPUT_DIR / "challenge_holdout"
FINAL_MODEL_DIR = HERE / "final_model" if (HERE / "final_model").exists() else HERE
MPL_CONFIG.mkdir(parents=True, exist_ok=True)
OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
INDIVIDUAL_DIR.mkdir(parents=True, exist_ok=True)
CHALLENGE_DIR.mkdir(parents=True, exist_ok=True)
os.environ.setdefault("MPLCONFIGDIR", str(MPL_CONFIG))
if MPL_DEPS.exists():
    sys.path.insert(0, str(MPL_DEPS))

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib import font_manager
from matplotlib.colors import Normalize
import numpy as np


METRICS_PATH = HERE / "oof_metrics.csv"
CALIBRATION_PATH = HERE / "site16_oof_calibration.json"
FONT_PATH = Path(r"C:\Windows\Fonts\msjh.ttc")

BLUE = "#2F6B9A"
BLUE_DARK = "#163A59"
BLUE_LIGHT = "#DCEAF4"
ORANGE = "#D97732"
INK = "#1F2937"
MUTED = "#667085"
GRID = "#D9DEE5"
PAPER = "#FFFFFF"
OUTSIDE = "#ECEFF3"
DISPLAY_MAX_RATIO = 2.0


if FONT_PATH.exists():
    font_manager.fontManager.addfont(str(FONT_PATH))
    FONT_NAME = font_manager.FontProperties(fname=str(FONT_PATH)).get_name()
else:
    FONT_NAME = "DejaVu Sans"

plt.rcParams.update(
    {
        "font.family": FONT_NAME,
        "font.size": 10.5,
        "axes.titlesize": 14,
        "axes.titleweight": "bold",
        "axes.labelsize": 11,
        "axes.edgecolor": "#AAB2BD",
        "axes.linewidth": 0.8,
        "axes.unicode_minus": False,
        "xtick.color": INK,
        "ytick.color": INK,
        "text.color": INK,
        "figure.facecolor": PAPER,
        "axes.facecolor": PAPER,
        "savefig.facecolor": PAPER,
        "savefig.bbox": "tight",
    }
)


def read_metrics() -> list[dict[str, str]]:
    with METRICS_PATH.open(newline="", encoding="utf-8-sig") as handle:
        rows = list(csv.DictReader(handle))
    rows.sort(key=lambda row: row["sample_id"])
    if len(rows) != 16:
        raise RuntimeError(f"Expected 16 OOF rows, found {len(rows)}")
    return rows


def load_oof_arrays() -> dict[str, dict[str, np.ndarray | int]]:
    result: dict[str, dict[str, np.ndarray | int]] = {}
    for path in sorted((HERE / "folds").glob("fold_*/oof/*_oof.npz")):
        with np.load(path, allow_pickle=False) as data:
            sample_id = str(data["sample_id"].item())
            result[sample_id] = {
                "fold": int(data["fold"].item()),
                "residual": data["tuned_residual"].astype(np.float32),
                "mask": data["label_mask"].astype(bool),
            }
    if len(result) != 16:
        raise RuntimeError(f"Expected 16 OOF arrays, found {len(result)}")
    return result


def masked_crop(
    residual: np.ndarray,
    mask: np.ndarray,
    calibration_p99: float,
    padding: int = 18,
) -> np.ndarray:
    ys, xs = np.where(mask)
    if not ys.size:
        raise RuntimeError("Empty OOF mask")
    y0 = max(0, int(ys.min()) - padding)
    y1 = min(mask.shape[0], int(ys.max()) + padding + 1)
    x0 = max(0, int(xs.min()) - padding)
    x1 = min(mask.shape[1], int(xs.max()) + padding + 1)
    ratio = residual / max(calibration_p99, 1e-12)
    ratio = np.where(mask & np.isfinite(ratio), ratio, np.nan)
    return ratio[y0:y1, x0:x1]


def heatmap_cmap():
    cmap = matplotlib.colormaps["magma"].copy()
    cmap.set_bad(OUTSIDE)
    return cmap


def save_individual(
    row: dict[str, str],
    arrays: dict[str, np.ndarray | int],
    calibration_p99: float,
) -> None:
    sample_id = row["sample_id"]
    fold = int(row["fold"]) + 1
    uniformity = float(row["tuned_uniformity_new_oof_p99"])
    mean_residual = float(row["tuned_mean_residual"])
    hot_area = 100.0 * float(row["tuned_hot_area_new_oof_p99"])
    improvement = -float(row["residual_change_percent"])
    image = masked_crop(
        arrays["residual"], arrays["mask"], calibration_p99  # type: ignore[arg-type]
    )

    fig = plt.figure(figsize=(6.2, 8.5))
    ax = fig.add_axes([0.055, 0.055, 0.70, 0.77])
    cax = fig.add_axes([0.835, 0.16, 0.035, 0.58])
    rendered = ax.imshow(
        image,
        cmap=heatmap_cmap(),
        norm=Normalize(vmin=0.0, vmax=DISPLAY_MAX_RATIO),
        interpolation="nearest",
        origin="upper",
        aspect="equal",
    )
    fig.text(0.055, 0.965, sample_id, ha="left", va="top", fontsize=17, fontweight="bold")
    fig.text(
        0.055,
        0.918,
        (
            f"Fold {fold} sample-level OOF｜均勻度 {uniformity:.1f}%｜"
            f"平均殘差 {mean_residual:.3e}"
        ),
        ha="left",
        va="top",
        fontsize=10.2,
        color=MUTED,
    )
    fig.text(
        0.055,
        0.884,
        f"相較原模型殘差降低 {improvement:.1f}%｜超過 OOF p99 面積 {hot_area:.2f}%",
        ha="left",
        va="top",
        fontsize=9.8,
        color=MUTED,
    )
    fig.text(
        0.055,
        0.852,
        "遮罩：最大 8-connected component",
        ha="left",
        va="top",
        fontsize=9.8,
        color=MUTED,
    )
    ax.set_xticks([])
    ax.set_yticks([])
    for spine in ax.spines.values():
        spine.set_visible(False)
    cbar = fig.colorbar(rendered, cax=cax)
    cbar.set_label("殘差 ÷ OOF p99（1.0 = 校正門檻）")
    cbar.set_ticks([0.0, 0.5, 1.0, 1.5, 2.0])
    cbar.ax.set_yticklabels(["0", "0.5", "1.0", "1.5", "2.0+"])
    fig.savefig(
        INDIVIDUAL_DIR / f"{sample_id}_OOF_residual_heatmap.png",
        dpi=300,
        bbox_inches=None,
    )
    plt.close(fig)


def save_overview(
    rows: list[dict[str, str]],
    arrays_by_id: dict[str, dict[str, np.ndarray | int]],
    calibration_p99: float,
) -> None:
    fig, axes = plt.subplots(4, 4, figsize=(15.2, 15.6))
    fig.subplots_adjust(left=0.035, right=0.92, bottom=0.055, top=0.91, wspace=0.08, hspace=0.27)
    rendered = None
    for ax, row in zip(axes.flat, rows):
        sample_id = row["sample_id"]
        fold = int(row["fold"]) + 1
        uniformity = float(row["tuned_uniformity_new_oof_p99"])
        arrays = arrays_by_id[sample_id]
        image = masked_crop(
            arrays["residual"], arrays["mask"], calibration_p99  # type: ignore[arg-type]
        )
        rendered = ax.imshow(
            image,
            cmap=heatmap_cmap(),
            norm=Normalize(vmin=0.0, vmax=DISPLAY_MAX_RATIO),
            interpolation="nearest",
            origin="upper",
            aspect="equal",
        )
        ax.set_title(f"{sample_id}｜Fold {fold}｜U {uniformity:.1f}%", fontsize=11.5, pad=6)
        ax.set_xticks([])
        ax.set_yticks([])
        for spine in ax.spines.values():
            spine.set_visible(False)

    fig.suptitle("現場 16 張樣本：4-fold sample-level OOF 殘差熱圖", fontsize=20, fontweight="bold", x=0.035, ha="left")
    fig.text(
        0.035,
        0.935,
        (
            "每張圖都由未看過該樣本的 fold 模型輸出；共用色階為殘差 ÷ OOF p99。"
            "灰色為遮罩外，1.0 代表 OOF 校正門檻。"
        ),
        ha="left",
        va="bottom",
        fontsize=11.5,
        color=MUTED,
    )
    if rendered is None:
        raise RuntimeError("No overview image was rendered")
    cax = fig.add_axes([0.94, 0.12, 0.018, 0.70])
    cbar = fig.colorbar(rendered, cax=cax)
    cbar.set_label("殘差 ÷ OOF p99", fontsize=11)
    cbar.set_ticks([0.0, 0.5, 1.0, 1.5, 2.0])
    cbar.ax.set_yticklabels(["0", "0.5", "1.0", "1.5", "2.0+"])
    fig.text(
        0.035,
        0.018,
        "均勻度 U = 100 x [1 - min(遮罩內平均殘差 / OOF p99, 1)]；OOF p99 = "
        f"{calibration_p99:.5e}",
        fontsize=10.5,
        color=MUTED,
    )
    fig.savefig(OUTPUT_DIR / "OOF_16_residual_heatmap_overview.png", dpi=300)
    plt.close(fig)


def save_summary(rows: list[dict[str, str]]) -> None:
    labels = [row["sample_id"].replace("FX17_", "") for row in rows]
    uniformity = np.asarray(
        [float(row["tuned_uniformity_new_oof_p99"]) for row in rows]
    )
    base = np.asarray([float(row["base_mean_residual"]) for row in rows]) * 1e5
    tuned = np.asarray([float(row["tuned_mean_residual"]) for row in rows]) * 1e5
    y = np.arange(len(rows))

    fig, (ax_u, ax_r) = plt.subplots(1, 2, figsize=(16.0, 10.0), gridspec_kw={"width_ratios": [0.90, 1.35]})
    fig.subplots_adjust(left=0.07, right=0.97, bottom=0.10, top=0.82, wspace=0.25)
    fig.suptitle("現場 16 張樣本：OOF 均勻度與平均殘差", fontsize=20, fontweight="bold", x=0.07, y=0.97, ha="left")
    fig.text(
        0.07,
        0.885,
        "4-fold sample-level OOF；每張樣本的分數均由未參與該 fold 訓練的模型產生。",
        fontsize=11.5,
        color=MUTED,
        ha="left",
    )

    bars = ax_u.barh(y, uniformity, color=BLUE, edgecolor=BLUE_DARK, linewidth=0.7)
    ax_u.set_yticks(y, labels)
    ax_u.invert_yaxis()
    ax_u.set_xlim(0, 100)
    ax_u.set_xlabel("均勻度（%）")
    ax_u.set_title("Fine-tune OOF 均勻度", loc="left", pad=12)
    ax_u.grid(axis="x", color=GRID, linewidth=0.7)
    ax_u.set_axisbelow(True)
    for bar, value in zip(bars, uniformity):
        ax_u.text(value + 1.1, bar.get_y() + bar.get_height() / 2, f"{value:.1f}%", va="center", fontsize=9.5, fontweight="bold", color=BLUE_DARK)
    ax_u.spines[["top", "right"]].set_visible(False)

    width = 0.36
    ax_r.barh(y - width / 2, base, height=width, color=ORANGE, edgecolor="#8F3F12", linewidth=0.7, label="原模型")
    ax_r.barh(y + width / 2, tuned, height=width, color=BLUE, edgecolor=BLUE_DARK, linewidth=0.7, label="Fine-tune OOF")
    ax_r.set_yticks(y, labels)
    ax_r.invert_yaxis()
    ax_r.set_xlim(0, max(base) * 1.16)
    ax_r.set_xlabel("遮罩內平均殘差 (x1e-5)")
    ax_r.set_title("原模型與 Fine-tune OOF 平均殘差", loc="left", pad=12)
    ax_r.grid(axis="x", color=GRID, linewidth=0.7)
    ax_r.set_axisbelow(True)
    ax_r.legend(frameon=False, loc="lower right")
    ax_r.spines[["top", "right"]].set_visible(False)

    mean_reduction = 100.0 * (1.0 - tuned.mean() / base.mean())
    fig.text(
        0.07,
        0.035,
        f"16 張平均：OOF 均勻度 {uniformity.mean():.1f}%；平均殘差由 {base.mean():.3f} x1e-5 降至 {tuned.mean():.3f} x1e-5（降低 {mean_reduction:.1f}%）。",
        fontsize=11,
        color=MUTED,
    )
    fig.savefig(OUTPUT_DIR / "OOF_uniformity_and_residual_comparison.png", dpi=300)
    plt.close(fig)


def save_report_csv(rows: list[dict[str, str]]) -> None:
    fields = [
        "sample_id",
        "fold_1_based",
        "label_pixels",
        "base_mean_residual",
        "oof_mean_residual",
        "residual_reduction_percent",
        "oof_uniformity_percent",
        "oof_hot_area_percent",
    ]
    output_rows = []
    for row in rows:
        output_rows.append(
            {
                "sample_id": row["sample_id"],
                "fold_1_based": int(row["fold"]) + 1,
                "label_pixels": int(row["label_pixels"]),
                "base_mean_residual": float(row["base_mean_residual"]),
                "oof_mean_residual": float(row["tuned_mean_residual"]),
                "residual_reduction_percent": -float(row["residual_change_percent"]),
                "oof_uniformity_percent": float(row["tuned_uniformity_new_oof_p99"]),
                "oof_hot_area_percent": 100.0 * float(row["tuned_hot_area_new_oof_p99"]),
            }
        )
    with (OUTPUT_DIR / "OOF_uniformity_table.csv").open("w", newline="", encoding="utf-8-sig") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields)
        writer.writeheader()
        writer.writerows(output_rows)


def load_challenge_data() -> tuple[list[dict[str, str]], dict[str, dict[str, np.ndarray]]]:
    metrics_path = FINAL_MODEL_DIR / "evaluation_metrics.csv"
    with metrics_path.open(newline="", encoding="utf-8-sig") as handle:
        rows = [
            row
            for row in csv.DictReader(handle)
            if row["sample_id"] in {"FX17_0003", "FX17_0018"}
        ]
    rows.sort(key=lambda row: row["sample_id"])
    arrays_by_id: dict[str, dict[str, np.ndarray]] = {}
    for row in rows:
        sample_id = row["sample_id"]
        with np.load(FINAL_MODEL_DIR / "evaluation" / f"{sample_id}.npz", allow_pickle=False) as data:
            arrays_by_id[sample_id] = {
                "residual": data["tuned_residual"].astype(np.float32),
                "mask": data["label_mask"].astype(bool),
            }
    if len(rows) != 2 or len(arrays_by_id) != 2:
        raise RuntimeError("Expected challenge holdouts FX17_0003 and FX17_0018")
    return rows, arrays_by_id


def save_challenge_individual(
    row: dict[str, str],
    arrays: dict[str, np.ndarray],
    calibration_p99: float,
) -> None:
    sample_id = row["sample_id"]
    uniformity = float(row["tuned_uniformity_new_oof_p99"])
    mean_residual = float(row["tuned_mean_residual"])
    hot_area = 100.0 * float(row["tuned_hot_area_new_oof_p99"])
    improvement = -float(row["residual_change_percent"])
    image = masked_crop(arrays["residual"], arrays["mask"], calibration_p99)

    fig = plt.figure(figsize=(6.2, 8.5))
    ax = fig.add_axes([0.055, 0.055, 0.70, 0.77])
    cax = fig.add_axes([0.835, 0.16, 0.035, 0.58])
    rendered = ax.imshow(
        image,
        cmap=heatmap_cmap(),
        norm=Normalize(vmin=0.0, vmax=DISPLAY_MAX_RATIO),
        interpolation="nearest",
        origin="upper",
        aspect="equal",
    )
    fig.text(0.055, 0.965, sample_id, ha="left", va="top", fontsize=17, fontweight="bold")
    fig.text(
        0.055,
        0.918,
        f"Challenge holdout（非 OOF fold）｜均勻度 {uniformity:.1f}%｜平均殘差 {mean_residual:.3e}",
        ha="left",
        va="top",
        fontsize=9.8,
        color=MUTED,
    )
    fig.text(
        0.055,
        0.884,
        f"相較原模型殘差降低 {improvement:.1f}%｜超過 OOF p99 面積 {hot_area:.2f}%",
        ha="left",
        va="top",
        fontsize=9.8,
        color=MUTED,
    )
    fig.text(
        0.055,
        0.852,
        "模型：全部 16 張乾淨樣本 fine-tune｜遮罩：最大 8-connected component",
        ha="left",
        va="top",
        fontsize=9.2,
        color=MUTED,
    )
    ax.set_xticks([])
    ax.set_yticks([])
    for spine in ax.spines.values():
        spine.set_visible(False)
    cbar = fig.colorbar(rendered, cax=cax)
    cbar.set_label("殘差 / OOF p99（1.0 = 校正門檻）")
    cbar.set_ticks([0.0, 0.5, 1.0, 1.5, 2.0])
    cbar.ax.set_yticklabels(["0", "0.5", "1.0", "1.5", "2.0+"])
    fig.savefig(
        CHALLENGE_DIR / f"{sample_id}_challenge_residual_heatmap.png",
        dpi=300,
        bbox_inches=None,
    )
    plt.close(fig)


def save_challenge_overview(
    rows: list[dict[str, str]],
    arrays_by_id: dict[str, dict[str, np.ndarray]],
    calibration_p99: float,
) -> None:
    fig, axes = plt.subplots(1, 2, figsize=(11.0, 7.6))
    fig.subplots_adjust(left=0.045, right=0.90, bottom=0.09, top=0.82, wspace=0.16)
    rendered = None
    for ax, row in zip(axes, rows):
        sample_id = row["sample_id"]
        uniformity = float(row["tuned_uniformity_new_oof_p99"])
        hot_area = 100.0 * float(row["tuned_hot_area_new_oof_p99"])
        image = masked_crop(
            arrays_by_id[sample_id]["residual"],
            arrays_by_id[sample_id]["mask"],
            calibration_p99,
        )
        rendered = ax.imshow(
            image,
            cmap=heatmap_cmap(),
            norm=Normalize(vmin=0.0, vmax=DISPLAY_MAX_RATIO),
            interpolation="nearest",
            origin="upper",
            aspect="equal",
        )
        ax.set_title(f"{sample_id}｜U {uniformity:.1f}%｜hot {hot_area:.2f}%", fontsize=12, pad=8)
        ax.set_xticks([])
        ax.set_yticks([])
        for spine in ax.spines.values():
            spine.set_visible(False)
    fig.suptitle("FX17_0003 與 FX17_0018：Challenge holdout 殘差熱圖", fontsize=19, fontweight="bold", x=0.045, y=0.96, ha="left")
    fig.text(
        0.045,
        0.875,
        "兩張樣本均未參與 4-fold 或最終 16 張模型訓練；以 clean-16 最終模型輸出，色階沿用 OOF p99。",
        fontsize=11,
        color=MUTED,
        ha="left",
    )
    if rendered is None:
        raise RuntimeError("No challenge image was rendered")
    cax = fig.add_axes([0.925, 0.18, 0.018, 0.52])
    cbar = fig.colorbar(rendered, cax=cax)
    cbar.set_label("殘差 / OOF p99")
    cbar.set_ticks([0.0, 0.5, 1.0, 1.5, 2.0])
    cbar.ax.set_yticklabels(["0", "0.5", "1.0", "1.5", "2.0+"])
    fig.text(
        0.045,
        0.025,
        "灰色為遮罩外；1.0 為 OOF 校正門檻。兩張均使用最大 8-connected component 遮罩。",
        fontsize=10.5,
        color=MUTED,
    )
    fig.savefig(CHALLENGE_DIR / "FX17_0003_0018_challenge_overview.png", dpi=300)
    plt.close(fig)


def main() -> None:
    rows = read_metrics()
    arrays_by_id = load_oof_arrays()
    calibration = json.loads(CALIBRATION_PATH.read_text(encoding="utf-8"))
    calibration_p99 = float(calibration["calibration_pixel_p99"])
    for row in rows:
        save_individual(row, arrays_by_id[row["sample_id"]], calibration_p99)
    save_overview(rows, arrays_by_id, calibration_p99)
    save_summary(rows)
    save_report_csv(rows)
    challenge_rows, challenge_arrays = load_challenge_data()
    for row in challenge_rows:
        save_challenge_individual(row, challenge_arrays[row["sample_id"]], calibration_p99)
    save_challenge_overview(challenge_rows, challenge_arrays, calibration_p99)
    print(f"[done] {OUTPUT_DIR}")


if __name__ == "__main__":
    main()
