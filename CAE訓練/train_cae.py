from __future__ import annotations

import argparse
import csv
import json
import math
import random
import time
from dataclasses import asdict, dataclass
from pathlib import Path

import numpy as np

from auto_sam_mask import (
    EXCLUDED_FOR_TRAINING,
    apply_colormap,
    build_sample_mask,
    choose_label_seed,
    choose_sam_mask,
    load_envi_bil,
    normalize01,
    overlay_mask,
    pseudo_rgb,
    sam_degrees,
    write_png,
)

try:
    import torch
    import torch.nn as nn
    import torch.nn.functional as F
    from torch.utils.data import DataLoader, Dataset
except Exception as exc:  # pragma: no cover - keeps --help usable without torch.
    torch = None
    nn = None
    F = None
    DataLoader = None
    Dataset = object
    TORCH_IMPORT_ERROR = exc
else:
    TORCH_IMPORT_ERROR = None


@dataclass
class ImageRecord:
    stem: str
    thickness: str
    hdr_path: str
    split: str


@dataclass
class PcaModel:
    band_mean: np.ndarray
    band_std: np.ndarray
    components: np.ndarray
    explained_variance_ratio: np.ndarray


class PatchDataset(Dataset):
    def __init__(self, inputs: np.ndarray, targets: np.ndarray, masks: np.ndarray) -> None:
        self.inputs = inputs.astype(np.float32, copy=False)
        self.targets = targets.astype(np.float32, copy=False)
        self.masks = masks.astype(np.float32, copy=False)

    def __len__(self) -> int:
        return int(self.inputs.shape[0])

    def __getitem__(self, index: int):
        return self.inputs[index], self.targets[index], self.masks[index]


def require_torch() -> None:
    if torch is None:
        raise RuntimeError(
            "PyTorch is required for CAE training. Install a CUDA-enabled torch build in your "
            f"training environment first. Original import error: {TORCH_IMPORT_ERROR}"
        )


def seed_everything(seed: int) -> None:
    random.seed(seed)
    np.random.seed(seed)
    if torch is not None:
        torch.manual_seed(seed)
        if torch.cuda.is_available():
            torch.cuda.manual_seed_all(seed)


def save_torch_checkpoint(payload: dict, path: Path) -> None:
    """Save through a Python file handle to avoid old PyTorch Windows Unicode path issues."""
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("wb") as f:
        torch.save(payload, f)


def load_torch_checkpoint(path: Path, map_location):
    """Load through a Python file handle for the same reason as save_torch_checkpoint."""
    with path.open("rb") as f:
        return torch.load(f, map_location=map_location)


def natural_key(path: Path) -> tuple[str, int]:
    stem = path.stem
    prefix, _, rest = stem.partition("_")
    number_text = rest.split("_", 1)[0]
    try:
        number = int(number_text)
    except ValueError:
        number = 9999
    return prefix, number


def discover_images(data_dir: Path, thickness: str, include_excluded: bool) -> list[Path]:
    headers = sorted(data_dir.glob("*_RT.hdr"), key=natural_key)
    if thickness != "all":
        headers = [p for p in headers if p.stem.startswith(thickness + "_")]
    if not include_excluded:
        headers = [p for p in headers if p.stem not in EXCLUDED_FOR_TRAINING]
    return headers


def stratified_split(headers: list[Path], seed: int, val_ratio: float, test_ratio: float) -> list[ImageRecord]:
    rng = random.Random(seed)
    by_thickness: dict[str, list[Path]] = {}
    for hdr_path in headers:
        thickness = hdr_path.stem.split("_", 1)[0]
        by_thickness.setdefault(thickness, []).append(hdr_path)

    records: list[ImageRecord] = []
    for thickness, group in sorted(by_thickness.items()):
        group = list(group)
        rng.shuffle(group)
        n = len(group)
        if n < 3:
            raise ValueError(f"Need at least 3 images for {thickness}, got {n}.")
        val_n = 0 if val_ratio <= 0 else max(1, int(round(n * val_ratio)))
        test_n = 0 if test_ratio <= 0 else max(1, int(round(n * test_ratio)))
        if val_n + test_n >= n:
            val_n = 1
            test_n = 1
        train_n = n - val_n - test_n
        split_paths = {
            "train": group[:train_n],
            "val": group[train_n : train_n + val_n],
            "test": group[train_n + val_n :],
        }
        for split, paths in split_paths.items():
            for path in sorted(paths, key=natural_key):
                records.append(
                    ImageRecord(
                        stem=path.stem,
                        thickness=thickness,
                        hdr_path=str(path),
                        split=split,
                    )
                )
    return sorted(records, key=lambda r: (r.thickness, r.split, natural_key(Path(r.hdr_path))))


def stratified_kfold(
    headers: list[Path],
    seed: int,
    fold_index: int,
    fold_count: int,
) -> list[ImageRecord]:
    if fold_count < 2:
        raise ValueError("fold_count must be at least 2")
    if not 0 <= fold_index < fold_count:
        raise ValueError(f"fold_index must be in [0, {fold_count - 1}]")
    rng = random.Random(seed)
    by_thickness: dict[str, list[Path]] = {}
    for hdr_path in headers:
        thickness = hdr_path.stem.split("_", 1)[0]
        by_thickness.setdefault(thickness, []).append(hdr_path)

    records: list[ImageRecord] = []
    for thickness, group in sorted(by_thickness.items()):
        group = sorted(group, key=natural_key)
        rng.shuffle(group)
        if len(group) < fold_count:
            raise ValueError(
                f"Need at least {fold_count} images for {thickness}, got {len(group)}"
            )
        for index, path in enumerate(group):
            split = "val" if index % fold_count == fold_index else "train"
            records.append(
                ImageRecord(
                    stem=path.stem,
                    thickness=thickness,
                    hdr_path=str(path),
                    split=split,
                )
            )
    return sorted(records, key=lambda r: (r.thickness, r.split, natural_key(Path(r.hdr_path))))


def write_split_manifest(records: list[ImageRecord], run_dir: Path) -> None:
    run_dir.mkdir(parents=True, exist_ok=True)
    rows = [asdict(r) for r in records]
    (run_dir / "split_manifest.json").write_text(json.dumps(rows, ensure_ascii=False, indent=2), encoding="utf-8")
    with (run_dir / "split_manifest.csv").open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=["stem", "thickness", "split", "hdr_path"])
        writer.writeheader()
        writer.writerows(rows)


def find_label_mask(cube: np.ndarray, args: argparse.Namespace) -> tuple[np.ndarray, dict]:
    mean_image = cube.mean(axis=2)
    sample_mask = build_sample_mask(mean_image)
    signal = normalize01(mean_image, sample_mask)
    seed, polarity, seed_info = choose_label_seed(
        signal=signal,
        sample_mask=sample_mask,
        label_polarity=args.label_polarity,
        seed_percentile=args.seed_percentile,
    )
    reference = cube[seed].mean(axis=0)
    sam = sam_degrees(cube, reference)
    label_mask, sam_threshold = choose_sam_mask(
        sam=sam,
        sample_mask=sample_mask,
        threshold_deg=args.sam_threshold_deg,
        max_label_fraction=args.max_label_fraction,
    )
    meta = {
        "label_polarity": polarity,
        "sam_threshold_deg": float(sam_threshold),
        "seed_pixels": int(seed_info["seed_pixels"]),
        "sample_pixels": int(seed_info["sample_pixels"]),
        "label_pixels": int(label_mask.sum()),
        "label_fraction_of_sample": float(label_mask.sum() / max(sample_mask.sum(), 1)),
    }
    return label_mask.astype(bool), meta


def sample_label_pixels(
    records: list[ImageRecord],
    max_pixels_total: int,
    args: argparse.Namespace,
) -> tuple[np.ndarray, list[dict]]:
    train_records = [r for r in records if r.split == "train"]
    per_image = max(500, math.ceil(max_pixels_total / max(len(train_records), 1)))
    rng = np.random.default_rng(args.seed)
    samples: list[np.ndarray] = []
    mask_summaries: list[dict] = []

    for record in train_records:
        cube, _, cube_meta = load_envi_bil(Path(record.hdr_path), trim_bands=args.trim_bands)
        mask, mask_meta = find_label_mask(cube, args)
        label_pixels = cube[mask]
        if label_pixels.shape[0] == 0:
            raise ValueError(f"No label pixels found for {record.stem}.")
        take = min(per_image, label_pixels.shape[0])
        idx = rng.choice(label_pixels.shape[0], size=take, replace=False)
        samples.append(label_pixels[idx])
        mask_summaries.append(
            {
                "stem": record.stem,
                "split": record.split,
                "thickness": record.thickness,
                **mask_meta,
                "lines": cube_meta["lines"],
                "samples": cube_meta["samples"],
                "bands": cube_meta["bands"],
            }
        )

    all_samples = np.concatenate(samples, axis=0)
    if all_samples.shape[0] > max_pixels_total:
        idx = rng.choice(all_samples.shape[0], size=max_pixels_total, replace=False)
        all_samples = all_samples[idx]
    return all_samples.astype(np.float32, copy=False), mask_summaries


def fit_pca(records: list[ImageRecord], args: argparse.Namespace, run_dir: Path) -> PcaModel:
    print("[pca] sampling train label pixels")
    samples, mask_summaries = sample_label_pixels(records, args.max_pca_pixels, args)

    band_mean = samples.mean(axis=0)
    band_std = samples.std(axis=0)
    band_std = np.where(band_std < 1e-6, 1.0, band_std)
    standardized = (samples - band_mean) / band_std

    print(f"[pca] fitting PCA from {standardized.shape[0]} pixels x {standardized.shape[1]} bands")
    _, singular_values, vt = np.linalg.svd(standardized, full_matrices=False)
    components = vt[: args.pca_components].astype(np.float32)
    variances = singular_values**2
    explained_variance_ratio = (variances / np.maximum(variances.sum(), 1e-12))[: args.pca_components]

    pca = PcaModel(
        band_mean=band_mean.astype(np.float32),
        band_std=band_std.astype(np.float32),
        components=components,
        explained_variance_ratio=explained_variance_ratio.astype(np.float32),
    )
    np.savez(
        run_dir / "pca_model.npz",
        band_mean=pca.band_mean,
        band_std=pca.band_std,
        components=pca.components,
        explained_variance_ratio=pca.explained_variance_ratio,
    )
    (run_dir / "mask_summary_train.json").write_text(
        json.dumps(mask_summaries, ensure_ascii=False, indent=2),
        encoding="utf-8",
    )
    print("[pca] explained variance ratio:", np.round(pca.explained_variance_ratio, 4).tolist())
    print("[pca] cumulative:", float(np.sum(pca.explained_variance_ratio)))
    return pca


def transform_cube_to_pca(cube: np.ndarray, pca: PcaModel) -> np.ndarray:
    h, w, bands = cube.shape
    flat = cube.reshape(-1, bands)
    standardized = (flat - pca.band_mean) / pca.band_std
    projected = standardized @ pca.components.T
    return projected.reshape(h, w, pca.components.shape[0]).astype(np.float32)


def patch_positions(mask: np.ndarray, patch_size: int, stride: int, min_mask_fraction: float) -> list[tuple[int, int]]:
    ys, xs = np.where(mask)
    if len(ys) == 0:
        return []
    margin = patch_size // 2
    y0 = max(0, int(ys.min()) - margin)
    y1 = min(mask.shape[0], int(ys.max()) + margin + 1)
    x0 = max(0, int(xs.min()) - margin)
    x1 = min(mask.shape[1], int(xs.max()) + margin + 1)

    if y1 - y0 < patch_size:
        y0 = max(0, min(y0, mask.shape[0] - patch_size))
        y1 = min(mask.shape[0], y0 + patch_size)
    if x1 - x0 < patch_size:
        x0 = max(0, min(x0, mask.shape[1] - patch_size))
        x1 = min(mask.shape[1], x0 + patch_size)

    y_positions = list(range(y0, max(y0, y1 - patch_size) + 1, stride))
    x_positions = list(range(x0, max(x0, x1 - patch_size) + 1, stride))
    if not y_positions or y_positions[-1] != y1 - patch_size:
        y_positions.append(max(0, y1 - patch_size))
    if not x_positions or x_positions[-1] != x1 - patch_size:
        x_positions.append(max(0, x1 - patch_size))

    positions = []
    for y in sorted(set(y_positions)):
        for x in sorted(set(x_positions)):
            mask_patch = mask[y : y + patch_size, x : x + patch_size]
            if mask_patch.mean() >= min_mask_fraction:
                positions.append((y, x))
    return positions


def extract_patches_from_image(
    pca_cube: np.ndarray,
    label_mask: np.ndarray,
    args: argparse.Namespace,
) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    positions = patch_positions(label_mask, args.patch_size, args.patch_stride, args.min_mask_fraction)
    inputs: list[np.ndarray] = []
    targets: list[np.ndarray] = []
    masks: list[np.ndarray] = []

    for y, x in positions:
        feature_patch = pca_cube[y : y + args.patch_size, x : x + args.patch_size]
        mask_patch = label_mask[y : y + args.patch_size, x : x + args.patch_size].astype(np.float32)
        target = np.transpose(feature_patch, (2, 0, 1))
        masked_feature = target * mask_patch[None, :, :]
        if args.include_mask_channel:
            model_input = np.concatenate([masked_feature, mask_patch[None, :, :]], axis=0)
        else:
            model_input = masked_feature
        inputs.append(model_input.astype(np.float32))
        targets.append(target.astype(np.float32))
        masks.append(mask_patch[None, :, :].astype(np.float32))

    if not inputs:
        channels = pca_cube.shape[2] + (1 if args.include_mask_channel else 0)
        return (
            np.empty((0, channels, args.patch_size, args.patch_size), dtype=np.float32),
            np.empty((0, pca_cube.shape[2], args.patch_size, args.patch_size), dtype=np.float32),
            np.empty((0, 1, args.patch_size, args.patch_size), dtype=np.float32),
        )
    return np.stack(inputs), np.stack(targets), np.stack(masks)


def build_patch_arrays(
    records: list[ImageRecord],
    pca: PcaModel,
    args: argparse.Namespace,
    run_dir: Path,
) -> tuple[dict[str, tuple[np.ndarray, np.ndarray, np.ndarray]], list[dict]]:
    arrays: dict[str, list[np.ndarray]] = {"train_inputs": [], "train_targets": [], "train_masks": []}
    arrays.update({"val_inputs": [], "val_targets": [], "val_masks": []})
    arrays.update({"test_inputs": [], "test_targets": [], "test_masks": []})
    summaries: list[dict] = []

    for record in records:
        print(f"[patch] {record.split:5s} {record.stem}")
        cube, wavelengths, cube_meta = load_envi_bil(Path(record.hdr_path), trim_bands=args.trim_bands)
        label_mask, mask_meta = find_label_mask(cube, args)
        pca_cube = transform_cube_to_pca(cube, pca)
        inputs, targets, masks = extract_patches_from_image(pca_cube, label_mask, args)

        arrays[f"{record.split}_inputs"].append(inputs)
        arrays[f"{record.split}_targets"].append(targets)
        arrays[f"{record.split}_masks"].append(masks)

        preview_dir = run_dir / "mask_previews" / record.stem
        preview_dir.mkdir(parents=True, exist_ok=True)
        rgb = pseudo_rgb(cube, wavelengths)
        write_png(preview_dir / "sam_overlay.png", overlay_mask(rgb, label_mask))
        write_png(preview_dir / "sam_mask.png", np.where(label_mask, 255, 0).astype(np.uint8))

        summaries.append(
            {
                "stem": record.stem,
                "split": record.split,
                "thickness": record.thickness,
                "patch_count": int(inputs.shape[0]),
                **mask_meta,
                "lines": cube_meta["lines"],
                "samples": cube_meta["samples"],
                "bands": cube_meta["bands"],
            }
        )

    result: dict[str, tuple[np.ndarray, np.ndarray, np.ndarray]] = {}
    for split in ["train", "val", "test"]:
        if arrays[f"{split}_inputs"]:
            split_inputs = np.concatenate(arrays[f"{split}_inputs"], axis=0)
            split_targets = np.concatenate(arrays[f"{split}_targets"], axis=0)
            split_masks = np.concatenate(arrays[f"{split}_masks"], axis=0)
        else:
            pca_channels = int(pca.components.shape[0])
            input_channels = pca_channels + (1 if args.include_mask_channel else 0)
            patch_shape = (args.patch_size, args.patch_size)
            split_inputs = np.empty((0, input_channels, *patch_shape), dtype=np.float32)
            split_targets = np.empty((0, pca_channels, *patch_shape), dtype=np.float32)
            split_masks = np.empty((0, 1, *patch_shape), dtype=np.float32)
        result[split] = (split_inputs, split_targets, split_masks)
        print(f"[patch] {split}: {split_inputs.shape[0]} patches")
        if args.save_patches:
            np.savez_compressed(
                run_dir / f"patches_{split}.npz",
                inputs=split_inputs,
                targets=split_targets,
                masks=split_masks,
            )

    (run_dir / "patch_summary.json").write_text(json.dumps(summaries, ensure_ascii=False, indent=2), encoding="utf-8")
    return result, summaries


def make_model(input_channels: int, output_channels: int, base_channels: int):
    require_torch()

    class ConvBlock(nn.Module):
        def __init__(self, in_channels: int, out_channels: int, stride: int = 1) -> None:
            super().__init__()
            groups = min(8, out_channels)
            self.block = nn.Sequential(
                nn.Conv2d(in_channels, out_channels, kernel_size=3, stride=stride, padding=1),
                nn.GroupNorm(groups, out_channels),
                nn.SiLU(inplace=True),
                nn.Conv2d(out_channels, out_channels, kernel_size=3, padding=1),
                nn.GroupNorm(groups, out_channels),
                nn.SiLU(inplace=True),
            )

        def forward(self, x):
            return self.block(x)

    class ConvAutoencoder(nn.Module):
        def __init__(self) -> None:
            super().__init__()
            c = base_channels
            self.encoder = nn.Sequential(
                ConvBlock(input_channels, c, stride=1),
                ConvBlock(c, c * 2, stride=2),
                ConvBlock(c * 2, c * 4, stride=2),
                ConvBlock(c * 4, c * 4, stride=1),
            )
            self.decoder = nn.Sequential(
                nn.ConvTranspose2d(c * 4, c * 2, kernel_size=4, stride=2, padding=1),
                nn.GroupNorm(min(8, c * 2), c * 2),
                nn.SiLU(inplace=True),
                ConvBlock(c * 2, c * 2, stride=1),
                nn.ConvTranspose2d(c * 2, c, kernel_size=4, stride=2, padding=1),
                nn.GroupNorm(min(8, c), c),
                nn.SiLU(inplace=True),
                ConvBlock(c, c, stride=1),
                nn.Conv2d(c, output_channels, kernel_size=1),
            )

        def forward(self, x):
            return self.decoder(self.encoder(x))

    return ConvAutoencoder()


def masked_mse(pred, target, mask):
    sq = (pred - target) ** 2
    denom = torch.clamp(mask.sum() * pred.shape[1], min=1.0)
    return (sq * mask).sum() / denom


def run_epoch(model, loader, optimizer, scaler, device, use_amp: bool, train: bool) -> float:
    require_torch()
    model.train(train)
    total_loss = 0.0
    total_items = 0

    for inputs, targets, masks in loader:
        inputs = inputs.to(device, non_blocking=True)
        targets = targets.to(device, non_blocking=True)
        masks = masks.to(device, non_blocking=True)

        with torch.set_grad_enabled(train):
            with torch.autocast(device_type="cuda", enabled=use_amp and device.type == "cuda"):
                pred = model(inputs)
                loss = masked_mse(pred, targets, masks)

            if train:
                optimizer.zero_grad(set_to_none=True)
                if scaler is not None:
                    scaler.scale(loss).backward()
                    scaler.unscale_(optimizer)
                    torch.nn.utils.clip_grad_norm_(model.parameters(), max_norm=2.0)
                    scaler.step(optimizer)
                    scaler.update()
                else:
                    loss.backward()
                    torch.nn.utils.clip_grad_norm_(model.parameters(), max_norm=2.0)
                    optimizer.step()

        batch_size = int(inputs.shape[0])
        total_loss += float(loss.detach().cpu()) * batch_size
        total_items += batch_size
    return total_loss / max(total_items, 1)


def train_model(
    patch_arrays: dict[str, tuple[np.ndarray, np.ndarray, np.ndarray]],
    args: argparse.Namespace,
    run_dir: Path,
    pca: PcaModel,
) -> Path:
    require_torch()
    run_dir.mkdir(parents=True, exist_ok=True)
    device = torch.device(args.device if args.device else ("cuda" if torch.cuda.is_available() else "cpu"))
    if device.type == "cuda":
        torch.set_float32_matmul_precision("high")
        print(f"[torch] CUDA device: {torch.cuda.get_device_name(device)}")
    else:
        print("[torch] CUDA not available; training on CPU")

    train_inputs, train_targets, train_masks = patch_arrays["train"]
    val_inputs, val_targets, val_masks = patch_arrays["val"]
    if train_inputs.shape[0] == 0:
        raise ValueError("Training split has no patches")
    has_validation = val_inputs.shape[0] > 0
    input_channels = int(train_inputs.shape[1])
    output_channels = int(train_targets.shape[1])

    model = make_model(input_channels, output_channels, args.base_channels).to(device)
    if args.channels_last and device.type == "cuda":
        model = model.to(memory_format=torch.channels_last)

    train_loader = DataLoader(
        PatchDataset(train_inputs, train_targets, train_masks),
        batch_size=args.batch_size,
        shuffle=True,
        num_workers=args.num_workers,
        pin_memory=device.type == "cuda",
        drop_last=False,
    )
    val_loader = None
    if has_validation:
        val_loader = DataLoader(
            PatchDataset(val_inputs, val_targets, val_masks),
            batch_size=args.batch_size,
            shuffle=False,
            num_workers=args.num_workers,
            pin_memory=device.type == "cuda",
            drop_last=False,
        )

    optimizer = torch.optim.AdamW(model.parameters(), lr=args.lr, weight_decay=args.weight_decay)
    scheduler = (
        torch.optim.lr_scheduler.ReduceLROnPlateau(
            optimizer,
            mode="min",
            factor=0.5,
            patience=5,
        )
        if has_validation
        else None
    )
    try:
        scaler = torch.amp.GradScaler("cuda", enabled=args.amp and device.type == "cuda")
    except (AttributeError, TypeError):
        scaler = torch.cuda.amp.GradScaler(enabled=args.amp and device.type == "cuda")

    history = []
    best_val = float("inf")
    best_epoch = 0
    best_path = run_dir / "best_cae.pt"
    best_path.parent.mkdir(parents=True, exist_ok=True)

    for epoch in range(1, args.epochs + 1):
        t0 = time.time()
        train_loss = run_epoch(model, train_loader, optimizer, scaler, device, args.amp, train=True)
        val_loss = (
            run_epoch(
                model,
                val_loader,
                optimizer,
                scaler=None,
                device=device,
                use_amp=args.amp,
                train=False,
            )
            if val_loader is not None
            else float("nan")
        )
        if scheduler is not None:
            scheduler.step(val_loss)
        lr = float(optimizer.param_groups[0]["lr"])
        elapsed = time.time() - t0
        row = {
            "epoch": epoch,
            "train_loss": train_loss,
            "val_loss": val_loss,
            "lr": lr,
            "seconds": elapsed,
        }
        history.append(row)
        val_text = f"{val_loss:.6f}" if has_validation else "n/a"
        print(
            f"[epoch {epoch:03d}] train={train_loss:.6f} val={val_text} "
            f"lr={lr:.2e} time={elapsed:.1f}s"
        )

        monitor_loss = val_loss if has_validation else train_loss
        if monitor_loss < best_val or not has_validation:
            best_val = monitor_loss
            best_epoch = epoch
            best_path.parent.mkdir(parents=True, exist_ok=True)
            save_torch_checkpoint(
                {
                    "model_state": model.state_dict(),
                    "input_channels": input_channels,
                    "output_channels": output_channels,
                    "base_channels": args.base_channels,
                    "patch_size": args.patch_size,
                    "pca_components": int(pca.components.shape[0]),
                    "include_mask_channel": args.include_mask_channel,
                    "best_val_loss": best_val,
                    "best_epoch": best_epoch,
                    "validation_mode": "held_out" if has_validation else "none_final_fit",
                },
                best_path,
            )
        elif has_validation and epoch - best_epoch >= args.patience:
            print(f"[early-stop] no validation improvement for {args.patience} epochs")
            break

    with (run_dir / "history.csv").open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=["epoch", "train_loss", "val_loss", "lr", "seconds"])
        writer.writeheader()
        writer.writerows(history)

    metric_name = "val_loss" if has_validation else "final_train_loss"
    print(f"[model] best epoch={best_epoch}, {metric_name}={best_val:.6f}")
    print(f"[model] saved: {best_path}")
    return best_path


def sliding_windows_for_mask(mask: np.ndarray, patch_size: int, stride: int) -> list[tuple[int, int]]:
    ys, xs = np.where(mask)
    if len(ys) == 0:
        return []
    margin = patch_size // 2
    y0 = max(0, int(ys.min()) - margin)
    y1 = min(mask.shape[0], int(ys.max()) + margin + 1)
    x0 = max(0, int(xs.min()) - margin)
    x1 = min(mask.shape[1], int(xs.max()) + margin + 1)
    y_positions = list(range(y0, max(y0, y1 - patch_size) + 1, stride))
    x_positions = list(range(x0, max(x0, x1 - patch_size) + 1, stride))
    if not y_positions or y_positions[-1] != y1 - patch_size:
        y_positions.append(max(0, y1 - patch_size))
    if not x_positions or x_positions[-1] != x1 - patch_size:
        x_positions.append(max(0, x1 - patch_size))
    return [(max(0, y), max(0, x)) for y in sorted(set(y_positions)) for x in sorted(set(x_positions))]


def residual_map_for_image(
    model,
    spectral_cube: np.ndarray,
    pca_cube: np.ndarray,
    label_mask: np.ndarray,
    pca: PcaModel,
    args: argparse.Namespace,
    device,
):
    require_torch()
    model.eval()
    h, w, k = pca_cube.shape
    residual_sum = np.zeros((h, w), dtype=np.float32)
    count = np.zeros((h, w), dtype=np.float32)
    windows = sliding_windows_for_mask(label_mask, args.patch_size, args.eval_stride)

    batch_inputs = []
    batch_positions = []

    def flush_batch() -> None:
        nonlocal batch_inputs, batch_positions, residual_sum, count
        if not batch_inputs:
            return
        x_np = np.stack(batch_inputs).astype(np.float32)
        with torch.no_grad():
            x = torch.from_numpy(x_np).to(device)
            with torch.autocast(device_type="cuda", enabled=args.amp and device.type == "cuda"):
                pred = model(x).float().cpu().numpy()
        for pred_patch, input_patch, (y, x0) in zip(pred, batch_inputs, batch_positions):
            target_pca = np.transpose(pca_cube[y : y + args.patch_size, x0 : x0 + args.patch_size], (2, 0, 1))
            mask_patch = label_mask[y : y + args.patch_size, x0 : x0 + args.patch_size].astype(np.float32)
            if args.residual_space == "spectral":
                pred_hwk = np.transpose(pred_patch, (1, 2, 0)).reshape(-1, pca.components.shape[0])
                recon_z = pred_hwk @ pca.components
                recon_spectral = recon_z * pca.band_std + pca.band_mean
                target_spectral = spectral_cube[
                    y : y + args.patch_size,
                    x0 : x0 + args.patch_size,
                    :,
                ].reshape(-1, spectral_cube.shape[2])
                err = ((recon_spectral - target_spectral) ** 2).mean(axis=1).reshape(
                    args.patch_size,
                    args.patch_size,
                )
            else:
                err = ((pred_patch - target_pca) ** 2).mean(axis=0)
            err = err * mask_patch
            residual_sum[y : y + args.patch_size, x0 : x0 + args.patch_size] += err
            count[y : y + args.patch_size, x0 : x0 + args.patch_size] += mask_patch
        batch_inputs = []
        batch_positions = []

    for y, x0 in windows:
        feature_patch = np.transpose(pca_cube[y : y + args.patch_size, x0 : x0 + args.patch_size], (2, 0, 1))
        mask_patch = label_mask[y : y + args.patch_size, x0 : x0 + args.patch_size].astype(np.float32)
        masked_feature = feature_patch * mask_patch[None, :, :]
        if args.include_mask_channel:
            model_input = np.concatenate([masked_feature, mask_patch[None, :, :]], axis=0)
        else:
            model_input = masked_feature
        batch_inputs.append(model_input)
        batch_positions.append((y, x0))
        if len(batch_inputs) >= args.batch_size:
            flush_batch()
    flush_batch()

    residual = residual_sum / np.maximum(count, 1e-6)
    residual[~label_mask] = np.nan
    return residual


FONT_5X7 = {
    "A": ["01110", "10001", "10001", "11111", "10001", "10001", "10001"],
    "B": ["11110", "10001", "10001", "11110", "10001", "10001", "11110"],
    "C": ["01111", "10000", "10000", "10000", "10000", "10000", "01111"],
    "D": ["11110", "10001", "10001", "10001", "10001", "10001", "11110"],
    "E": ["11111", "10000", "10000", "11110", "10000", "10000", "11111"],
    "F": ["11111", "10000", "10000", "11110", "10000", "10000", "10000"],
    "G": ["01111", "10000", "10000", "10111", "10001", "10001", "01111"],
    "H": ["10001", "10001", "10001", "11111", "10001", "10001", "10001"],
    "I": ["11111", "00100", "00100", "00100", "00100", "00100", "11111"],
    "J": ["00111", "00010", "00010", "00010", "10010", "10010", "01100"],
    "K": ["10001", "10010", "10100", "11000", "10100", "10010", "10001"],
    "L": ["10000", "10000", "10000", "10000", "10000", "10000", "11111"],
    "M": ["10001", "11011", "10101", "10101", "10001", "10001", "10001"],
    "N": ["10001", "11001", "10101", "10011", "10001", "10001", "10001"],
    "O": ["01110", "10001", "10001", "10001", "10001", "10001", "01110"],
    "P": ["11110", "10001", "10001", "11110", "10000", "10000", "10000"],
    "Q": ["01110", "10001", "10001", "10001", "10101", "10010", "01101"],
    "R": ["11110", "10001", "10001", "11110", "10100", "10010", "10001"],
    "S": ["01111", "10000", "10000", "01110", "00001", "00001", "11110"],
    "T": ["11111", "00100", "00100", "00100", "00100", "00100", "00100"],
    "U": ["10001", "10001", "10001", "10001", "10001", "10001", "01110"],
    "V": ["10001", "10001", "10001", "10001", "10001", "01010", "00100"],
    "W": ["10001", "10001", "10001", "10101", "10101", "10101", "01010"],
    "X": ["10001", "10001", "01010", "00100", "01010", "10001", "10001"],
    "Y": ["10001", "10001", "01010", "00100", "00100", "00100", "00100"],
    "Z": ["11111", "00001", "00010", "00100", "01000", "10000", "11111"],
    "0": ["01110", "10001", "10011", "10101", "11001", "10001", "01110"],
    "1": ["00100", "01100", "00100", "00100", "00100", "00100", "01110"],
    "2": ["01110", "10001", "00001", "00010", "00100", "01000", "11111"],
    "3": ["11110", "00001", "00001", "01110", "00001", "00001", "11110"],
    "4": ["00010", "00110", "01010", "10010", "11111", "00010", "00010"],
    "5": ["11111", "10000", "10000", "11110", "00001", "00001", "11110"],
    "6": ["01110", "10000", "10000", "11110", "10001", "10001", "01110"],
    "7": ["11111", "00001", "00010", "00100", "01000", "01000", "01000"],
    "8": ["01110", "10001", "10001", "01110", "10001", "10001", "01110"],
    "9": ["01110", "10001", "10001", "01111", "00001", "00001", "01110"],
    " ": ["00000", "00000", "00000", "00000", "00000", "00000", "00000"],
    ".": ["00000", "00000", "00000", "00000", "00000", "01100", "01100"],
    ":": ["00000", "01100", "01100", "00000", "01100", "01100", "00000"],
    "%": ["11001", "11010", "00010", "00100", "01000", "01011", "10011"],
    "-": ["00000", "00000", "00000", "11111", "00000", "00000", "00000"],
    "_": ["00000", "00000", "00000", "00000", "00000", "00000", "11111"],
    "/": ["00001", "00010", "00010", "00100", "01000", "01000", "10000"],
}


def draw_text(image: np.ndarray, text: str, x: int, y: int, color=(20, 20, 20), scale: int = 3) -> None:
    cursor = x
    for char in text.upper():
        pattern = FONT_5X7.get(char, FONT_5X7[" "])
        for row_idx, row in enumerate(pattern):
            for col_idx, bit in enumerate(row):
                if bit == "1":
                    y0 = y + row_idx * scale
                    x0 = cursor + col_idx * scale
                    image[y0 : y0 + scale, x0 : x0 + scale] = color
        cursor += 6 * scale


def save_result_summary_matplotlib(
    output_dir: Path,
    stem: str,
    rgb: np.ndarray,
    residual: np.ndarray,
    label_mask: np.ndarray,
    uniformity_score: float,
    mean_residual: float,
    p95_residual: float,
    hot_area_ratio: float,
) -> bool:
    try:
        import matplotlib

        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except Exception:
        return False

    finite = np.isfinite(residual) & label_mask
    residual_view = np.full_like(residual, np.nan, dtype=np.float32)
    residual_view[finite] = residual[finite]
    if finite.any():
        vmin, vmax = np.percentile(residual[finite], [1, 99])
        if vmax <= vmin:
            vmin, vmax = float(np.nanmin(residual_view)), float(np.nanmax(residual_view))
    else:
        vmin, vmax = 0.0, 1.0

    plt.rcParams["font.sans-serif"] = ["Microsoft JhengHei", "Arial", "DejaVu Sans"]
    plt.rcParams["axes.unicode_minus"] = False
    fig, axes = plt.subplots(1, 2, figsize=(12, 7), dpi=160, constrained_layout=True)
    fig.suptitle(
        f"{stem} | Uniformity {uniformity_score:.1f}%",
        fontsize=18,
        fontweight="bold",
    )
    fig.text(
        0.5,
        0.925,
        f"Mean residual: {mean_residual:.4g}    P95: {p95_residual:.4g}    Hot area: {hot_area_ratio * 100:.2f}%",
        ha="center",
        va="center",
        fontsize=10,
    )

    axes[0].imshow(overlay_mask(rgb, label_mask))
    axes[0].set_title("ROI overlay")
    axes[0].axis("off")

    cmap = plt.get_cmap("inferno").copy()
    cmap.set_bad("black")
    im = axes[1].imshow(np.ma.masked_invalid(residual_view), cmap=cmap, vmin=vmin, vmax=vmax)
    axes[1].set_title("Residual map")
    axes[1].axis("off")
    cbar = fig.colorbar(im, ax=axes[1], fraction=0.046, pad=0.04)
    cbar.set_label("Reconstruction error")

    fig.savefig(output_dir / "result_summary.png", bbox_inches="tight")
    plt.close(fig)
    return True


def residual_to_heatmap(residual: np.ndarray, label_mask: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    finite = np.isfinite(residual) & label_mask
    norm = np.zeros_like(residual, dtype=np.float32)
    if finite.any():
        lo, hi = np.percentile(residual[finite], [1, 99])
        if hi > lo:
            norm[finite] = np.clip((residual[finite] - lo) / (hi - lo), 0.0, 1.0)
    heat = apply_colormap(norm)
    heat[~label_mask] = [0, 0, 0]
    return heat, norm


def save_result_summary(
    output_dir: Path,
    stem: str,
    rgb: np.ndarray,
    residual: np.ndarray,
    label_mask: np.ndarray,
    uniformity_score: float,
    mean_residual: float,
    p95_residual: float,
    hot_area_ratio: float,
) -> None:
    output_dir.mkdir(parents=True, exist_ok=True)
    if save_result_summary_matplotlib(
        output_dir=output_dir,
        stem=stem,
        rgb=rgb,
        residual=residual,
        label_mask=label_mask,
        uniformity_score=uniformity_score,
        mean_residual=mean_residual,
        p95_residual=p95_residual,
        hot_area_ratio=hot_area_ratio,
    ):
        return

    overlay = overlay_mask(rgb, label_mask)
    heat, _ = residual_to_heatmap(residual, label_mask)
    overlay_u8 = np.clip(overlay, 0, 255).astype(np.uint8)
    heat_u8 = np.clip(heat, 0, 255).astype(np.uint8)

    h, w = overlay_u8.shape[:2]
    gap = 24
    top = 96
    bottom = 42
    canvas = np.full((top + h + bottom, w * 2 + gap, 3), 255, dtype=np.uint8)
    canvas[top : top + h, 0:w] = overlay_u8
    canvas[top : top + h, w + gap : w * 2 + gap] = heat_u8

    draw_text(canvas, f"SAMPLE {stem}", 16, 14, scale=3)
    draw_text(canvas, f"UNIFORMITY {uniformity_score:05.1f}%", 16, 48, color=(0, 95, 60), scale=4)
    draw_text(
        canvas,
        f"MEAN {mean_residual:.4G}  P95 {p95_residual:.4G}  HOT {hot_area_ratio * 100:.2F}%",
        520,
        56,
        color=(40, 40, 40),
        scale=2,
    )
    draw_text(canvas, "ROI OVERLAY", 16, top + h + 12, scale=2)
    draw_text(canvas, "RESIDUAL MAP", w + gap + 16, top + h + 12, scale=2)
    write_png(output_dir / "result_summary.png", canvas)


def save_residual_images(output_dir: Path, residual: np.ndarray, label_mask: np.ndarray, rgb: np.ndarray) -> None:
    output_dir.mkdir(parents=True, exist_ok=True)
    heat, _ = residual_to_heatmap(residual, label_mask)
    write_png(output_dir / "residual_heatmap.png", heat)
    write_png(output_dir / "label_mask.png", np.where(label_mask, 255, 0).astype(np.uint8))
    write_png(output_dir / "label_overlay.png", overlay_mask(rgb, label_mask))


def evaluate_model(
    records: list[ImageRecord],
    pca: PcaModel,
    best_path: Path,
    args: argparse.Namespace,
    run_dir: Path,
) -> None:
    require_torch()
    device = torch.device(args.device if args.device else ("cuda" if torch.cuda.is_available() else "cpu"))
    checkpoint = load_torch_checkpoint(best_path, map_location=device)
    model = make_model(
        input_channels=int(checkpoint["input_channels"]),
        output_channels=int(checkpoint["output_channels"]),
        base_channels=int(checkpoint["base_channels"]),
    ).to(device)
    model.load_state_dict(checkpoint["model_state"])

    metric_rows = []
    calibration_split = "val" if any(record.split == "val" for record in records) else "train"
    calibration_pixel_residuals = []

    for pass_name in ["calibration", "final"]:
        for record in records:
            cube, wavelengths, _ = load_envi_bil(Path(record.hdr_path), trim_bands=args.trim_bands)
            label_mask, mask_meta = find_label_mask(cube, args)
            pca_cube = transform_cube_to_pca(cube, pca)
            residual = residual_map_for_image(model, cube, pca_cube, label_mask, pca, args, device)
            finite = np.isfinite(residual) & label_mask

            if pass_name == "calibration":
                if record.split == calibration_split and finite.any():
                    calibration_pixel_residuals.append(residual[finite])
                continue

            if not finite.any():
                continue
            if not calibration_pixel_residuals:
                raise RuntimeError(f"No residual pixels found for calibration split {calibration_split}")
            all_calibration_pixels = np.concatenate(calibration_pixel_residuals)
            cal_p99 = float(np.percentile(all_calibration_pixels, 99))
            cal_p95 = float(np.percentile(all_calibration_pixels, 95))
            mean_residual = float(np.mean(residual[finite]))
            p95_residual = float(np.percentile(residual[finite], 95))
            hot_area_ratio = float(np.mean(residual[finite] > cal_p99))
            normalized_mean_error = min(mean_residual / max(cal_p99, 1e-12), 1.0)
            uniformity_score = 100.0 * (1.0 - normalized_mean_error)

            image_out = run_dir / "residual_outputs" / record.split / record.stem
            rgb = pseudo_rgb(cube, wavelengths)
            save_residual_images(image_out, residual, label_mask, rgb)
            save_result_summary(
                output_dir=image_out,
                stem=record.stem,
                rgb=rgb,
                residual=residual,
                label_mask=label_mask,
                uniformity_score=uniformity_score,
                mean_residual=mean_residual,
                p95_residual=p95_residual,
                hot_area_ratio=hot_area_ratio,
            )
            np.save(image_out / "residual_map.npy", np.nan_to_num(residual, nan=0.0).astype(np.float32))

            metric_rows.append(
                {
                    "stem": record.stem,
                    "thickness": record.thickness,
                    "split": record.split,
                    "mean_residual": mean_residual,
                    "p95_residual": p95_residual,
                    "calibration_pixel_p95": cal_p95,
                    "calibration_pixel_p99": cal_p99,
                    "calibration_split": calibration_split,
                    "hot_area_ratio_gt_val_p99": hot_area_ratio,
                    "normalized_mean_error": normalized_mean_error,
                    "uniformity_score_0_100": uniformity_score,
                    "residual_space": args.residual_space,
                    "label_fraction_of_sample": mask_meta["label_fraction_of_sample"],
                    "sam_threshold_deg": mask_meta["sam_threshold_deg"],
                }
            )

    with (run_dir / "residual_metrics.csv").open("w", newline="", encoding="utf-8") as f:
        fieldnames = [
            "stem",
            "thickness",
            "split",
            "mean_residual",
            "p95_residual",
            "calibration_pixel_p95",
            "calibration_pixel_p99",
            "calibration_split",
            "hot_area_ratio_gt_val_p99",
            "normalized_mean_error",
            "uniformity_score_0_100",
            "residual_space",
            "label_fraction_of_sample",
            "sam_threshold_deg",
        ]
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(metric_rows)
    print(f"[eval] wrote {run_dir / 'residual_metrics.csv'}")


def resolve_data_dir(script_dir: Path, user_data_dir: str | None) -> Path:
    candidates = []
    if user_data_dir:
        candidates.append(Path(user_data_dir))
    candidates.append(script_dir / "導電材料_RT")
    candidates.append(script_dir.parent / "20260528_導電材料" / "導電材料_RT")
    for candidate in candidates:
        if candidate.exists():
            return candidate.resolve()
    raise FileNotFoundError("Cannot find 導電材料_RT folder.")


def save_config(args: argparse.Namespace, records: list[ImageRecord], run_dir: Path) -> None:
    split_counts: dict[str, dict[str, int]] = {}
    for record in records:
        split_counts.setdefault(record.thickness, {}).setdefault(record.split, 0)
        split_counts[record.thickness][record.split] += 1
    config = {
        "args": vars(args),
        "excluded_for_training": sorted(EXCLUDED_FOR_TRAINING),
        "split_counts": split_counts,
    }
    (run_dir / "config.json").write_text(json.dumps(config, ensure_ascii=False, indent=2), encoding="utf-8")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Train a masked 2D convolutional autoencoder on conductive-label ROI patches.")
    parser.add_argument("--data-dir", default=None, help="Folder containing corrected *_RT.hdr/raw files.")
    parser.add_argument("--run-name", default=None, help="Run folder name under cae_runs.")
    parser.add_argument("--thickness", choices=["50u", "70u", "all"], default="all")
    parser.add_argument("--include-excluded", action="store_true", help="Include known HDR/RAW line-mismatch files.")
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--val-ratio", type=float, default=0.15)
    parser.add_argument("--test-ratio", type=float, default=0.15)
    parser.add_argument("--cv-fold-index", type=int, default=None)
    parser.add_argument("--cv-fold-count", type=int, default=5)

    parser.add_argument("--trim-bands", type=int, default=10)
    parser.add_argument("--pca-components", type=int, default=8)
    parser.add_argument("--max-pca-pixels", type=int, default=80000)
    parser.add_argument("--label-polarity", choices=["auto", "bright", "dark"], default="auto")
    parser.add_argument("--seed-percentile", type=float, default=85.0)
    parser.add_argument("--sam-threshold-deg", type=float, default=None)
    parser.add_argument("--max-label-fraction", type=float, default=0.32)

    parser.add_argument("--patch-size", type=int, default=32)
    parser.add_argument("--patch-stride", type=int, default=8)
    parser.add_argument("--eval-stride", type=int, default=16)
    parser.add_argument("--min-mask-fraction", type=float, default=0.15)
    parser.add_argument("--include-mask-channel", action=argparse.BooleanOptionalAction, default=True)

    parser.add_argument("--base-channels", type=int, default=32)
    parser.add_argument("--epochs", type=int, default=80)
    parser.add_argument("--batch-size", type=int, default=256)
    parser.add_argument("--lr", type=float, default=1e-3)
    parser.add_argument("--weight-decay", type=float, default=1e-5)
    parser.add_argument("--patience", type=int, default=12)
    parser.add_argument("--num-workers", type=int, default=0)
    parser.add_argument("--device", default=None, help="Example: cuda, cuda:0, or cpu.")
    parser.add_argument("--amp", action=argparse.BooleanOptionalAction, default=True)
    parser.add_argument("--channels-last", action=argparse.BooleanOptionalAction, default=True)
    parser.add_argument(
        "--save-patches",
        action=argparse.BooleanOptionalAction,
        default=True,
        help="Persist compressed patch arrays; disable for CV folds to avoid multi-GB I/O.",
    )
    parser.add_argument(
        "--residual-space",
        choices=["spectral", "pca"],
        default="spectral",
        help="spectral matches the proposal-style band-wise reconstruction error; pca uses PCA-channel MSE.",
    )
    parser.add_argument("--skip-eval", action="store_true")
    return parser


def main() -> None:
    args = build_parser().parse_args()
    require_torch()
    seed_everything(args.seed)

    script_dir = Path(__file__).resolve().parent
    data_dir = resolve_data_dir(script_dir, args.data_dir)
    timestamp = time.strftime("%Y%m%d_%H%M%S")
    run_name = args.run_name or f"cae_{args.thickness}_{timestamp}"
    run_dir = script_dir / "cae_runs" / run_name
    run_dir.mkdir(parents=True, exist_ok=True)

    print(f"[data] {data_dir}")
    print(f"[run] {run_dir}")
    print(f"[exclude] {', '.join(sorted(EXCLUDED_FOR_TRAINING))}")

    headers = discover_images(data_dir, args.thickness, args.include_excluded)
    if not headers:
        raise FileNotFoundError(f"No HDR files found for thickness={args.thickness} in {data_dir}")

    if args.cv_fold_index is not None:
        records = stratified_kfold(
            headers,
            seed=args.seed,
            fold_index=args.cv_fold_index,
            fold_count=args.cv_fold_count,
        )
    else:
        records = stratified_split(headers, args.seed, args.val_ratio, args.test_ratio)
    write_split_manifest(records, run_dir)
    save_config(args, records, run_dir)
    print("[split]")
    for record in records:
        print(f"  {record.split:5s} {record.thickness:3s} {record.stem}")

    pca = fit_pca(records, args, run_dir)
    patch_arrays, _ = build_patch_arrays(records, pca, args, run_dir)
    best_path = train_model(patch_arrays, args, run_dir, pca)

    if not args.skip_eval:
        evaluate_model(records, pca, best_path, args, run_dir)

    print("[done]")
    print(f"Run folder: {run_dir}")


if __name__ == "__main__":
    main()
