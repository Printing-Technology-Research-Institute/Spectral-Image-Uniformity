from __future__ import annotations

import argparse
import csv
import gc
import json
import random
import shutil
import sys
import time
from pathlib import Path
from types import SimpleNamespace

import cv2
import numpy as np


ROOT = Path(__file__).resolve().parent
BUNDLE = Path(r"C:\Users\HSIPL\Desktop\FX17_site_bundle_0812")
CAE_DIR = BUNDLE / "CAE訓練"
QT_DIR = BUNDLE / "動態分析模組" / "qt_demo"
SITE_DIR = BUNDLE / "sample_records" / "calibrated"
BASE_RUN = CAE_DIR / "cae_runs" / "cae_all_v3"
BASE_CHECKPOINT = BASE_RUN / "best_cae.pt"
BASE_PCA = BASE_RUN / "pca_model.npz"
BASE_CALIBRATION = CAE_DIR / "cae_runs" / "cae_all_v3_cv_calibration.json"

for directory in (str(QT_DIR), str(CAE_DIR)):
    if directory not in sys.path:
        sys.path.insert(0, directory)

import torch  # noqa: E402
from torch.utils.data import DataLoader  # noqa: E402

from auto_sam_mask import load_envi_bil  # noqa: E402
from infer_cae import load_pca, load_run_args  # noqa: E402
from live_analysis import find_label_mask_in_spatial_roi  # noqa: E402
from train_cae import (  # noqa: E402
    PatchDataset,
    extract_patches_from_image,
    find_label_mask,
    load_torch_checkpoint,
    make_model,
    residual_map_for_image,
    run_epoch,
    save_torch_checkpoint,
    seed_everything,
    transform_cube_to_pca,
)


EXCLUDED_SITE_IDS = {"FX17_0003", "FX17_0018"}
CHALLENGE_SITE_IDS = ["FX17_0003", "FX17_0018"]
FOLD_COUNT = 4
SEED = 42
PATCH_STRIDE = 8
EVAL_STRIDE = 16
EPOCHS = 30
PATIENCE = 6
LEARNING_RATE = 1e-4
BATCH_SIZE = 256
WEIGHT_DECAY = 1e-5
SPATIAL_ROI_START = 70
SPATIAL_ROI_END = 570

PREPARED_DIR = ROOT / "prepared_site16"
FOLDS_DIR = ROOT / "folds"
OOF_DIR = ROOT / "oof"
FINAL_DIR = ROOT / "final_model"
CALIBRATION_PATH = ROOT / "site16_oof_calibration.json"
ASSIGNMENTS_PATH = ROOT / "fold_assignments.json"


def latest_site_headers() -> dict[str, Path]:
    latest: dict[str, Path] = {}
    for path in SITE_DIR.glob("*_FX17_*.hdr"):
        sample_id = path.stem.rsplit("_", 2)[-2] + "_" + path.stem.rsplit("_", 1)[-1]
        previous = latest.get(sample_id)
        if previous is None or path.name > previous.name:
            latest[sample_id] = path
    expected = {f"FX17_{index:04d}" for index in range(1, 19)}
    if set(latest) != expected:
        missing = sorted(expected - set(latest))
        extra = sorted(set(latest) - expected)
        raise RuntimeError(f"Unexpected site sample set. missing={missing}, extra={extra}")
    return latest


def eligible_site_headers() -> dict[str, Path]:
    return {
        sample_id: path
        for sample_id, path in latest_site_headers().items()
        if sample_id not in EXCLUDED_SITE_IDS
    }


def build_run_args() -> SimpleNamespace:
    args = load_run_args(
        BASE_RUN,
        SimpleNamespace(data_dir=None, device="cuda", residual_space="spectral"),
    )
    args.trim_bands = 0
    args.patch_stride = PATCH_STRIDE
    args.eval_stride = EVAL_STRIDE
    args.batch_size = BATCH_SIZE
    args.device = "cuda"
    args.amp = True
    args.channels_last = True
    return args


def largest_8_connected_component(mask: np.ndarray) -> tuple[np.ndarray, dict]:
    count, labels, stats, _ = cv2.connectedComponentsWithStats(
        np.asarray(mask, dtype=np.uint8), connectivity=8
    )
    if count <= 1:
        return np.asarray(mask, dtype=bool), {
            "components_before_largest": 0,
            "largest_component_pixels": 0,
            "removed_component_pixels": 0,
        }
    sizes = stats[1:, cv2.CC_STAT_AREA].astype(np.int64)
    largest_index = int(np.argmax(sizes))
    largest_label = largest_index + 1
    largest_pixels = int(sizes[largest_index])
    return labels == largest_label, {
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


def site_mask(
    cube: np.ndarray,
    args: SimpleNamespace,
) -> tuple[np.ndarray, dict]:
    candidate, meta = find_label_mask_in_spatial_roi(
        cube,
        find_label_mask,
        args,
        start=SPATIAL_ROI_START,
        end=SPATIAL_ROI_END,
    )
    final_mask, largest_meta = largest_8_connected_component(candidate)
    if not final_mask.any():
        raise RuntimeError("Largest-component site mask is empty")
    result_meta = dict(meta)
    result_meta.update(largest_meta)
    result_meta["label_pixels"] = int(final_mask.sum())
    return final_mask, result_meta


def build_fold_assignments(sample_ids: list[str]) -> dict[str, object]:
    shuffled = sorted(sample_ids)
    random.Random(SEED).shuffle(shuffled)
    folds = [[] for _ in range(FOLD_COUNT)]
    for index, sample_id in enumerate(shuffled):
        folds[index % FOLD_COUNT].append(sample_id)
    payload = {
        "seed": SEED,
        "fold_count": FOLD_COUNT,
        "excluded_contaminated_samples": sorted(EXCLUDED_SITE_IDS),
        "eligible_sample_ids": sorted(sample_ids),
        "folds": [sorted(fold) for fold in folds],
    }
    ASSIGNMENTS_PATH.write_text(
        json.dumps(payload, ensure_ascii=False, indent=2), encoding="utf-8"
    )
    return payload


def load_fold_assignments() -> dict[str, object]:
    if not ASSIGNMENTS_PATH.exists():
        return build_fold_assignments(sorted(eligible_site_headers()))
    return json.loads(ASSIGNMENTS_PATH.read_text(encoding="utf-8"))


def prepared_paths(sample_id: str) -> tuple[Path, Path, Path]:
    return (
        PREPARED_DIR / f"{sample_id}_patches.npz",
        PREPARED_DIR / f"{sample_id}_mask.npy",
        PREPARED_DIR / f"{sample_id}_meta.json",
    )


def prepare_site16(force: bool = False) -> None:
    PREPARED_DIR.mkdir(parents=True, exist_ok=True)
    args = build_run_args()
    pca = load_pca(BASE_RUN)
    headers = eligible_site_headers()
    build_fold_assignments(sorted(headers))

    summaries: list[dict[str, object]] = []
    for index, (sample_id, header) in enumerate(sorted(headers.items()), start=1):
        patch_path, mask_path, meta_path = prepared_paths(sample_id)
        if patch_path.exists() and mask_path.exists() and meta_path.exists() and not force:
            meta = json.loads(meta_path.read_text(encoding="utf-8"))
            summaries.append(meta)
            print(f"[prepare {index:02d}/16] {sample_id} cached", flush=True)
            continue

        cube, _, cube_meta = load_envi_bil(header, trim_bands=0)
        if cube.shape[2] != pca.band_mean.shape[0]:
            raise ValueError(
                f"{sample_id}: cube has {cube.shape[2]} bands; PCA expects "
                f"{pca.band_mean.shape[0]}"
            )
        mask, mask_meta = site_mask(cube, args)
        pca_cube = transform_cube_to_pca(cube, pca)
        inputs, targets, masks = extract_patches_from_image(
            pca_cube, mask, args
        )
        if inputs.shape[0] == 0:
            raise RuntimeError(f"{sample_id}: no fine-tune patches")

        np.savez_compressed(
            patch_path,
            inputs=inputs,
            targets=targets,
            masks=masks,
        )
        np.save(mask_path, mask.astype(np.uint8))
        meta = {
            "sample_id": sample_id,
            "header_path": str(header),
            "cube_shape": [int(value) for value in cube.shape],
            "patch_count": int(inputs.shape[0]),
            "patch_stride": PATCH_STRIDE,
            "patch_size": int(args.patch_size),
            "patch_memory_mb": float(
                (inputs.nbytes + targets.nbytes + masks.nbytes) / 1048576.0
            ),
            **cube_meta,
            **mask_meta,
        }
        meta_path.write_text(
            json.dumps(meta, ensure_ascii=False, indent=2), encoding="utf-8"
        )
        summaries.append(meta)
        print(
            f"[prepare {index:02d}/16] {sample_id} patches={inputs.shape[0]} "
            f"mask={int(mask.sum())}",
            flush=True,
        )
        del cube, pca_cube, inputs, targets, masks, mask
        gc.collect()

    (ROOT / "prepared_summary.json").write_text(
        json.dumps(summaries, ensure_ascii=False, indent=2), encoding="utf-8"
    )
    print(
        f"[prepare done] samples={len(summaries)} "
        f"patches={sum(int(row['patch_count']) for row in summaries)}",
        flush=True,
    )


def load_patch_split(
    sample_ids: list[str],
) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    input_chunks: list[np.ndarray] = []
    target_chunks: list[np.ndarray] = []
    mask_chunks: list[np.ndarray] = []
    for sample_id in sample_ids:
        patch_path, _, _ = prepared_paths(sample_id)
        with np.load(patch_path) as arrays:
            input_chunks.append(arrays["inputs"])
            target_chunks.append(arrays["targets"])
            mask_chunks.append(arrays["masks"])
    result = (
        np.concatenate(input_chunks, axis=0),
        np.concatenate(target_chunks, axis=0),
        np.concatenate(mask_chunks, axis=0),
    )
    return result


def load_base_model(device: torch.device, channels_last: bool = True):
    checkpoint = load_torch_checkpoint(BASE_CHECKPOINT, map_location=device)
    model = make_model(
        input_channels=int(checkpoint["input_channels"]),
        output_channels=int(checkpoint["output_channels"]),
        base_channels=int(checkpoint["base_channels"]),
    ).to(device)
    model.load_state_dict(checkpoint["model_state"])
    if channels_last and device.type == "cuda":
        model = model.to(memory_format=torch.channels_last)
    return model, checkpoint


def make_loaders(
    train_arrays: tuple[np.ndarray, np.ndarray, np.ndarray],
    val_arrays: tuple[np.ndarray, np.ndarray, np.ndarray] | None,
) -> tuple[DataLoader, DataLoader | None]:
    train_loader = DataLoader(
        PatchDataset(*train_arrays),
        batch_size=BATCH_SIZE,
        shuffle=True,
        num_workers=0,
        pin_memory=True,
        drop_last=False,
    )
    val_loader = None
    if val_arrays is not None:
        val_loader = DataLoader(
            PatchDataset(*val_arrays),
            batch_size=BATCH_SIZE,
            shuffle=False,
            num_workers=0,
            pin_memory=True,
            drop_last=False,
        )
    return train_loader, val_loader


def make_scaler():
    try:
        return torch.amp.GradScaler("cuda", enabled=True)
    except (AttributeError, TypeError):
        return torch.cuda.amp.GradScaler(enabled=True)


def save_history(path: Path, rows: list[dict[str, object]]) -> None:
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(
            handle,
            fieldnames=["epoch", "train_loss", "val_loss", "lr", "seconds"],
        )
        writer.writeheader()
        writer.writerows(rows)


def train_fold(fold_index: int, force: bool = False) -> None:
    if not 0 <= fold_index < FOLD_COUNT:
        raise ValueError(f"fold_index must be 0..{FOLD_COUNT - 1}")
    prepare_site16(force=False)
    assignments = load_fold_assignments()
    val_ids = list(assignments["folds"][fold_index])
    train_ids = sorted(set(assignments["eligible_sample_ids"]) - set(val_ids))
    fold_dir = FOLDS_DIR / f"fold_{fold_index}"
    checkpoint_path = fold_dir / "best_cae.pt"
    complete_path = fold_dir / "complete.json"
    if complete_path.exists() and checkpoint_path.exists() and not force:
        print(f"[fold {fold_index}] already complete", flush=True)
        return
    fold_dir.mkdir(parents=True, exist_ok=True)
    (fold_dir / "split.json").write_text(
        json.dumps(
            {"fold": fold_index, "train": train_ids, "val": val_ids},
            ensure_ascii=False,
            indent=2,
        ),
        encoding="utf-8",
    )

    seed_everything(SEED + fold_index)
    print(
        f"[fold {fold_index}] loading patches train={len(train_ids)} val={len(val_ids)}",
        flush=True,
    )
    train_arrays = load_patch_split(train_ids)
    val_arrays = load_patch_split(val_ids)
    print(
        f"[fold {fold_index}] train_patches={train_arrays[0].shape[0]} "
        f"val_patches={val_arrays[0].shape[0]}",
        flush=True,
    )

    device = torch.device("cuda")
    torch.set_float32_matmul_precision("high")
    model, base_checkpoint = load_base_model(device)
    train_loader, val_loader = make_loaders(train_arrays, val_arrays)
    optimizer = torch.optim.AdamW(
        model.parameters(), lr=LEARNING_RATE, weight_decay=WEIGHT_DECAY
    )
    scheduler = torch.optim.lr_scheduler.ReduceLROnPlateau(
        optimizer, mode="min", factor=0.5, patience=3
    )
    scaler = make_scaler()
    pca = load_pca(BASE_RUN)

    history: list[dict[str, object]] = []
    best_val = float("inf")
    best_epoch = 0
    for epoch in range(1, EPOCHS + 1):
        started = time.time()
        train_loss = run_epoch(
            model, train_loader, optimizer, scaler, device, True, train=True
        )
        val_loss = run_epoch(
            model, val_loader, optimizer, None, device, True, train=False
        )
        scheduler.step(val_loss)
        lr = float(optimizer.param_groups[0]["lr"])
        elapsed = time.time() - started
        history.append(
            {
                "epoch": epoch,
                "train_loss": train_loss,
                "val_loss": val_loss,
                "lr": lr,
                "seconds": elapsed,
            }
        )
        print(
            f"[fold {fold_index} epoch {epoch:02d}] train={train_loss:.6f} "
            f"val={val_loss:.6f} lr={lr:.2e} time={elapsed:.1f}s",
            flush=True,
        )
        if val_loss < best_val:
            best_val = val_loss
            best_epoch = epoch
            save_torch_checkpoint(
                {
                    "model_state": model.state_dict(),
                    "input_channels": int(base_checkpoint["input_channels"]),
                    "output_channels": int(base_checkpoint["output_channels"]),
                    "base_channels": int(base_checkpoint["base_channels"]),
                    "patch_size": int(base_checkpoint["patch_size"]),
                    "pca_components": int(base_checkpoint["pca_components"]),
                    "include_mask_channel": bool(
                        base_checkpoint["include_mask_channel"]
                    ),
                    "best_val_loss": best_val,
                    "best_epoch": best_epoch,
                    "validation_mode": "site16_sample_level_oof",
                    "fine_tune_source_checkpoint": str(BASE_CHECKPOINT),
                    "fine_tune_fold": fold_index,
                    "fine_tune_train_ids": train_ids,
                    "fine_tune_val_ids": val_ids,
                    "fine_tune_lr": LEARNING_RATE,
                    "mask_strategy": "largest_8_connected_only",
                },
                checkpoint_path,
            )
        elif epoch - best_epoch >= PATIENCE:
            print(
                f"[fold {fold_index}] early stop at epoch={epoch}, best={best_epoch}",
                flush=True,
            )
            break

    save_history(fold_dir / "history.csv", history)
    del model, train_loader, val_loader, train_arrays, val_arrays
    gc.collect()
    torch.cuda.empty_cache()
    evaluate_fold(fold_index)
    complete_path.write_text(
        json.dumps(
            {
                "fold": fold_index,
                "best_epoch": best_epoch,
                "best_val_loss": best_val,
                "train_ids": train_ids,
                "val_ids": val_ids,
            },
            ensure_ascii=False,
            indent=2,
        ),
        encoding="utf-8",
    )
    print(
        f"[fold {fold_index} done] best_epoch={best_epoch} val={best_val:.6f}",
        flush=True,
    )


def load_checkpoint_model(path: Path, device: torch.device):
    checkpoint = load_torch_checkpoint(path, map_location=device)
    model = make_model(
        input_channels=int(checkpoint["input_channels"]),
        output_channels=int(checkpoint["output_channels"]),
        base_channels=int(checkpoint["base_channels"]),
    ).to(device)
    model.load_state_dict(checkpoint["model_state"])
    model = model.to(memory_format=torch.channels_last)
    model.eval()
    return model, checkpoint


def residual_values_for_sample(
    sample_id: str,
    header: Path,
    model,
    pca,
    args,
    device: torch.device,
) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    cube, _, _ = load_envi_bil(header, trim_bands=0)
    _, mask_path, _ = prepared_paths(sample_id)
    if mask_path.exists():
        mask = np.load(mask_path).astype(bool)
    else:
        mask, _ = site_mask(cube, args)
    pca_cube = transform_cube_to_pca(cube, pca)
    residual = residual_map_for_image(
        model, cube, pca_cube, mask, pca, args, device
    )
    values = residual[np.isfinite(residual) & mask].astype(np.float32)
    return values, residual.astype(np.float32), mask


def evaluate_fold(fold_index: int) -> None:
    fold_dir = FOLDS_DIR / f"fold_{fold_index}"
    split = json.loads((fold_dir / "split.json").read_text(encoding="utf-8"))
    val_ids = list(split["val"])
    headers = latest_site_headers()
    args = build_run_args()
    pca = load_pca(BASE_RUN)
    device = torch.device("cuda")
    tuned_model, _ = load_checkpoint_model(fold_dir / "best_cae.pt", device)
    base_model, _ = load_base_model(device)
    output_dir = fold_dir / "oof"
    output_dir.mkdir(parents=True, exist_ok=True)

    for sample_id in val_ids:
        tuned_values, tuned_map, mask = residual_values_for_sample(
            sample_id, headers[sample_id], tuned_model, pca, args, device
        )
        base_values, base_map, _ = residual_values_for_sample(
            sample_id, headers[sample_id], base_model, pca, args, device
        )
        np.savez_compressed(
            output_dir / f"{sample_id}_oof.npz",
            sample_id=np.asarray(sample_id),
            fold=np.asarray(fold_index, dtype=np.int64),
            tuned_values=tuned_values,
            base_values=base_values,
            tuned_residual=tuned_map,
            base_residual=base_map,
            label_mask=mask.astype(np.uint8),
        )
        print(
            f"[fold {fold_index} OOF] {sample_id} "
            f"base_mean={base_values.mean():.6g} tuned_mean={tuned_values.mean():.6g}",
            flush=True,
        )

    del tuned_model, base_model
    gc.collect()
    torch.cuda.empty_cache()


def metric_row(
    sample_id: str,
    fold: int | str,
    base_values: np.ndarray,
    tuned_values: np.ndarray,
    old_p99: float,
    new_p99: float,
    split: str,
) -> dict[str, object]:
    base_mean = float(base_values.mean())
    tuned_mean = float(tuned_values.mean())
    return {
        "sample_id": sample_id,
        "fold": fold,
        "split": split,
        "label_pixels": int(tuned_values.size),
        "base_mean_residual": base_mean,
        "tuned_mean_residual": tuned_mean,
        "residual_change_percent": 100.0 * (tuned_mean / base_mean - 1.0),
        "base_p95_residual": float(np.percentile(base_values, 95)),
        "tuned_p95_residual": float(np.percentile(tuned_values, 95)),
        "base_uniformity_old_p99": 100.0
        * (1.0 - min(base_mean / old_p99, 1.0)),
        "tuned_uniformity_old_p99": 100.0
        * (1.0 - min(tuned_mean / old_p99, 1.0)),
        "tuned_uniformity_new_oof_p99": 100.0
        * (1.0 - min(tuned_mean / new_p99, 1.0)),
        "tuned_hot_area_new_oof_p99": float(np.mean(tuned_values > new_p99)),
    }


def calibrate_oof() -> None:
    OOF_DIR.mkdir(parents=True, exist_ok=True)
    arrays = []
    for fold_index in range(FOLD_COUNT):
        complete = FOLDS_DIR / f"fold_{fold_index}" / "complete.json"
        if not complete.exists():
            raise RuntimeError(f"Fold {fold_index} is not complete")
        arrays.extend(
            sorted((FOLDS_DIR / f"fold_{fold_index}" / "oof").glob("*_oof.npz"))
        )
    if len(arrays) != 16:
        raise RuntimeError(f"Expected 16 OOF files, found {len(arrays)}")

    tuned_all: list[np.ndarray] = []
    base_all: list[np.ndarray] = []
    raw_rows: list[tuple[str, int, np.ndarray, np.ndarray]] = []
    seen: set[str] = set()
    for path in arrays:
        with np.load(path) as values:
            sample_id = str(values["sample_id"].item())
            fold = int(values["fold"].item())
            tuned = values["tuned_values"].astype(np.float32)
            base = values["base_values"].astype(np.float32)
        if sample_id in seen:
            raise RuntimeError(f"Duplicate OOF sample {sample_id}")
        seen.add(sample_id)
        tuned_all.append(tuned)
        base_all.append(base)
        raw_rows.append((sample_id, fold, base, tuned))

    tuned_pixels = np.concatenate(tuned_all)
    base_pixels = np.concatenate(base_all)
    tuned_p95 = float(np.percentile(tuned_pixels, 95))
    tuned_p99 = float(np.percentile(tuned_pixels, 99))
    base_clean16_p99 = float(np.percentile(base_pixels, 99))
    old_calibration = json.loads(BASE_CALIBRATION.read_text(encoding="utf-8"))
    old_p99 = float(old_calibration["calibration_pixel_p99"])
    best_epochs = [
        int(
            json.loads(
                (FOLDS_DIR / f"fold_{index}" / "complete.json").read_text(
                    encoding="utf-8"
                )
            )["best_epoch"]
        )
        for index in range(FOLD_COUNT)
    ]
    recommended_final_epochs = int(round(float(np.median(best_epochs))))

    rows = [
        metric_row(
            sample_id,
            fold,
            base,
            tuned,
            old_p99,
            tuned_p99,
            "oof",
        )
        for sample_id, fold, base, tuned in sorted(raw_rows)
    ]
    with (ROOT / "oof_metrics.csv").open(
        "w", newline="", encoding="utf-8-sig"
    ) as handle:
        writer = csv.DictWriter(handle, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)

    payload = {
        "calibration_pixel_p95": tuned_p95,
        "calibration_pixel_p99": tuned_p99,
        "calibration_source": "16 clean site samples, 4-fold sample-level OOF",
        "mask_strategy": "largest_8_connected_only",
        "sample_ids": sorted(seen),
        "excluded_contaminated_samples": sorted(EXCLUDED_SITE_IDS),
        "fold_count": FOLD_COUNT,
        "seed": SEED,
        "oof_pixel_count": int(tuned_pixels.size),
        "base_model_clean16_oof_pixel_p99": base_clean16_p99,
        "previous_calibration_pixel_p99": old_p99,
        "best_epochs_by_fold": best_epochs,
        "recommended_final_epochs": recommended_final_epochs,
        "mean_base_oof_residual": float(base_pixels.mean()),
        "mean_tuned_oof_residual": float(tuned_pixels.mean()),
        "mean_oof_residual_change_percent": float(
            100.0 * (tuned_pixels.mean() / base_pixels.mean() - 1.0)
        ),
    }
    CALIBRATION_PATH.write_text(
        json.dumps(payload, ensure_ascii=False, indent=2), encoding="utf-8"
    )
    np.savez_compressed(
        OOF_DIR / "pooled_oof_residual_pixels.npz",
        tuned=tuned_pixels,
        base=base_pixels,
    )
    print(json.dumps(payload, ensure_ascii=False, indent=2), flush=True)


def train_final(force: bool = False) -> None:
    if not CALIBRATION_PATH.exists():
        calibrate_oof()
    if (FINAL_DIR / "complete.json").exists() and not force:
        print("[final] already complete", flush=True)
        return
    prepare_site16(force=False)
    calibration = json.loads(CALIBRATION_PATH.read_text(encoding="utf-8"))
    epochs = int(calibration["recommended_final_epochs"])
    sample_ids = sorted(eligible_site_headers())
    final_arrays = load_patch_split(sample_ids)
    train_loader, _ = make_loaders(final_arrays, None)
    device = torch.device("cuda")
    seed_everything(SEED + 100)
    model, base_checkpoint = load_base_model(device)
    optimizer = torch.optim.AdamW(
        model.parameters(), lr=LEARNING_RATE, weight_decay=WEIGHT_DECAY
    )
    scaler = make_scaler()
    history: list[dict[str, object]] = []
    for epoch in range(1, epochs + 1):
        started = time.time()
        loss = run_epoch(
            model, train_loader, optimizer, scaler, device, True, train=True
        )
        elapsed = time.time() - started
        history.append(
            {
                "epoch": epoch,
                "train_loss": loss,
                "val_loss": "",
                "lr": LEARNING_RATE,
                "seconds": elapsed,
            }
        )
        print(
            f"[final epoch {epoch:02d}/{epochs:02d}] train={loss:.6f} "
            f"time={elapsed:.1f}s",
            flush=True,
        )

    FINAL_DIR.mkdir(parents=True, exist_ok=True)
    final_checkpoint = FINAL_DIR / "best_cae.pt"
    save_torch_checkpoint(
        {
            "model_state": model.state_dict(),
            "input_channels": int(base_checkpoint["input_channels"]),
            "output_channels": int(base_checkpoint["output_channels"]),
            "base_channels": int(base_checkpoint["base_channels"]),
            "patch_size": int(base_checkpoint["patch_size"]),
            "pca_components": int(base_checkpoint["pca_components"]),
            "include_mask_channel": bool(base_checkpoint["include_mask_channel"]),
            "best_val_loss": float(history[-1]["train_loss"]),
            "best_epoch": epochs,
            "validation_mode": "final_fit_all_16_after_4fold_oof",
            "fine_tune_source_checkpoint": str(BASE_CHECKPOINT),
            "fine_tune_train_ids": sample_ids,
            "fine_tune_lr": LEARNING_RATE,
            "mask_strategy": "largest_8_connected_only",
            "oof_calibration_path": str(CALIBRATION_PATH),
        },
        final_checkpoint,
    )
    shutil.copy2(BASE_PCA, FINAL_DIR / "pca_model.npz")
    shutil.copy2(CALIBRATION_PATH, FINAL_DIR / "site16_oof_calibration.json")
    save_history(FINAL_DIR / "history.csv", history)

    base_config = json.loads((BASE_RUN / "config.json").read_text(encoding="utf-8"))
    config_args = dict(base_config["args"])
    config_args.update(
        {
            "data_dir": str(SITE_DIR),
            "seed": SEED,
            "cv_fold_index": None,
            "cv_fold_count": FOLD_COUNT,
            "trim_bands": 0,
            "patch_stride": PATCH_STRIDE,
            "eval_stride": EVAL_STRIDE,
            "epochs": epochs,
            "batch_size": BATCH_SIZE,
            "lr": LEARNING_RATE,
            "weight_decay": WEIGHT_DECAY,
            "patience": PATIENCE,
            "device": "cuda",
            "amp": True,
            "channels_last": True,
            "residual_space": "spectral",
        }
    )
    config = {
        "args": config_args,
        "fine_tune": {
            "source_run": str(BASE_RUN),
            "sample_ids": sample_ids,
            "excluded_contaminated_samples": sorted(EXCLUDED_SITE_IDS),
            "mask_strategy": "largest_8_connected_only",
            "fold_count": FOLD_COUNT,
            "fold_assignments": str(ASSIGNMENTS_PATH),
            "calibration": str(CALIBRATION_PATH),
        },
    }
    (FINAL_DIR / "config.json").write_text(
        json.dumps(config, ensure_ascii=False, indent=2), encoding="utf-8"
    )
    del model, train_loader, final_arrays
    gc.collect()
    torch.cuda.empty_cache()
    evaluate_final_and_challenge()
    (FINAL_DIR / "complete.json").write_text(
        json.dumps(
            {
                "epochs": epochs,
                "sample_ids": sample_ids,
                "final_train_loss": history[-1]["train_loss"],
                "calibration_pixel_p99": calibration["calibration_pixel_p99"],
            },
            ensure_ascii=False,
            indent=2,
        ),
        encoding="utf-8",
    )
    print(f"[final done] {FINAL_DIR}", flush=True)


def evaluate_final_and_challenge() -> None:
    calibration = json.loads(CALIBRATION_PATH.read_text(encoding="utf-8"))
    new_p99 = float(calibration["calibration_pixel_p99"])
    old_p99 = float(
        json.loads(BASE_CALIBRATION.read_text(encoding="utf-8"))[
            "calibration_pixel_p99"
        ]
    )
    device = torch.device("cuda")
    pca = load_pca(BASE_RUN)
    args = build_run_args()
    tuned_model, _ = load_checkpoint_model(FINAL_DIR / "best_cae.pt", device)
    base_model, _ = load_base_model(device)
    headers = latest_site_headers()
    rows: list[dict[str, object]] = []
    for sample_id in sorted(headers):
        split = "challenge_contaminated" if sample_id in EXCLUDED_SITE_IDS else "final_train"
        tuned_values, tuned_map, mask = residual_values_for_sample(
            sample_id, headers[sample_id], tuned_model, pca, args, device
        )
        base_values, base_map, _ = residual_values_for_sample(
            sample_id, headers[sample_id], base_model, pca, args, device
        )
        rows.append(
            metric_row(
                sample_id,
                "final",
                base_values,
                tuned_values,
                old_p99,
                new_p99,
                split,
            )
        )
        output = FINAL_DIR / "evaluation" / f"{sample_id}.npz"
        output.parent.mkdir(parents=True, exist_ok=True)
        np.savez_compressed(
            output,
            tuned_residual=tuned_map,
            base_residual=base_map,
            label_mask=mask.astype(np.uint8),
        )
        print(
            f"[final eval] {sample_id} base={base_values.mean():.6g} "
            f"tuned={tuned_values.mean():.6g}",
            flush=True,
        )
    with (FINAL_DIR / "evaluation_metrics.csv").open(
        "w", newline="", encoding="utf-8-sig"
    ) as handle:
        writer = csv.DictWriter(handle, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)


def run_all(force: bool = False) -> None:
    prepare_site16(force=force)
    for fold_index in range(FOLD_COUNT):
        train_fold(fold_index, force=force)
    calibrate_oof()
    train_final(force=force)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Fine-tune cae_all_v3 on 16 clean site samples with 4-fold sample-level OOF."
    )
    parser.add_argument(
        "command",
        choices=["prepare", "fold", "calibrate", "final", "all"],
    )
    parser.add_argument("--fold-index", type=int, default=None)
    parser.add_argument("--force", action="store_true")
    return parser


def main() -> None:
    cli = build_parser().parse_args()
    ROOT.mkdir(parents=True, exist_ok=True)
    if not torch.cuda.is_available():
        raise RuntimeError("CUDA is required for this fine-tune run")
    print(
        f"[device] {torch.cuda.get_device_name(0)} torch={torch.__version__}",
        flush=True,
    )
    if cli.command == "prepare":
        prepare_site16(force=cli.force)
    elif cli.command == "fold":
        if cli.fold_index is None:
            raise ValueError("--fold-index is required for command=fold")
        train_fold(cli.fold_index, force=cli.force)
    elif cli.command == "calibrate":
        calibrate_oof()
    elif cli.command == "final":
        train_final(force=cli.force)
    else:
        run_all(force=cli.force)


if __name__ == "__main__":
    main()
