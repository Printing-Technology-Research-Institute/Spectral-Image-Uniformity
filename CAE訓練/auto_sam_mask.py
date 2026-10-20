from __future__ import annotations

import argparse
import html
import math
import re
import struct
import zlib
from pathlib import Path

import numpy as np


# RAW size and HDR line count do not match for these files. Keep them out of
# training/evaluation batches unless you explicitly fix their headers first.
EXCLUDED_FOR_TRAINING = {
    "50u_15_RT",
    "70u_1_RT",
    "70u_2_RT",
    "70u_6_RT",
    "70u_7_RT",
    "70u_12_RT",
}


ENVI_DTYPE = {
    1: np.uint8,
    2: np.int16,
    3: np.int32,
    4: np.float32,
    5: np.float64,
    12: np.uint16,
}


def parse_envi_header(hdr_path: Path) -> dict:
    text = hdr_path.read_text(encoding="utf-8", errors="ignore")
    meta: dict[str, object] = {}

    for key in [
        "samples",
        "lines",
        "bands",
        "header offset",
        "data type",
        "byte order",
        "interleave",
    ]:
        match = re.search(rf"(?im)^\s*{re.escape(key)}\s*=\s*([^\r\n]+)", text)
        if match:
            value = match.group(1).strip()
            meta[key] = value.lower() if key == "interleave" else int(value)

    wave_match = re.search(r"(?is)wavelength\s*=\s*\{(?P<body>.*?)\}", text)
    if wave_match:
        meta["wavelength"] = np.array(
            [float(m.group(0)) for m in re.finditer(r"[-+]?\d+(?:\.\d+)?", wave_match.group("body"))],
            dtype=np.float32,
        )

    missing = [k for k in ["samples", "lines", "bands", "data type", "interleave"] if k not in meta]
    if missing:
        raise ValueError(f"{hdr_path.name} missing required ENVI metadata: {', '.join(missing)}")

    if meta["interleave"] != "bil":
        raise ValueError(f"{hdr_path.name}: only BIL interleave is supported now, got {meta['interleave']!r}")

    return meta


def raw_path_for_header(hdr_path: Path) -> Path:
    raw_path = hdr_path.with_suffix(".raw")
    if not raw_path.exists():
        raise FileNotFoundError(f"Missing RAW file for {hdr_path.name}: {raw_path}")
    return raw_path


def load_envi_bil(hdr_path: Path, trim_bands: int = 10) -> tuple[np.ndarray, np.ndarray | None, dict]:
    meta = parse_envi_header(hdr_path)
    raw_path = raw_path_for_header(hdr_path)

    samples = int(meta["samples"])
    hdr_lines = int(meta["lines"])
    bands = int(meta["bands"])
    header_offset = int(meta.get("header offset", 0))
    dtype_code = int(meta["data type"])
    dtype = ENVI_DTYPE.get(dtype_code)
    if dtype is None:
        raise ValueError(f"{hdr_path.name}: unsupported ENVI data type {dtype_code}")

    byte_order = int(meta.get("byte order", 0))
    dtype = np.dtype(dtype).newbyteorder("<" if byte_order == 0 else ">")
    bytes_per_value = dtype.itemsize
    bytes_per_line = samples * bands * bytes_per_value
    raw_bytes = raw_path.stat().st_size - header_offset

    if raw_bytes % bytes_per_line != 0:
        raise ValueError(
            f"{hdr_path.name}: RAW size does not divide cleanly by samples*bands. "
            f"raw_bytes={raw_bytes}, bytes_per_line={bytes_per_line}"
        )

    actual_lines = raw_bytes // bytes_per_line
    if actual_lines != hdr_lines:
        print(
            f"[warn] {hdr_path.name}: HDR lines={hdr_lines}, RAW-implied lines={actual_lines}; "
            "using RAW-implied lines for this run."
        )

    memmap = np.memmap(
        raw_path,
        dtype=dtype,
        mode="r",
        offset=header_offset,
        shape=(actual_lines, bands, samples),
    )
    cube = np.asarray(memmap[:, trim_bands : bands - trim_bands, :], dtype=np.float32)
    cube = np.transpose(cube, (0, 2, 1))
    cube = np.nan_to_num(cube, nan=0.0, posinf=0.0, neginf=0.0)

    wavelengths = meta.get("wavelength")
    if isinstance(wavelengths, np.ndarray) and len(wavelengths) == bands:
        wavelengths = wavelengths[trim_bands : bands - trim_bands]
    else:
        wavelengths = None

    resolved_meta = {
        "samples": samples,
        "lines": int(actual_lines),
        "hdr_lines": hdr_lines,
        "bands": cube.shape[2],
        "source_bands": bands,
        "trim_bands": trim_bands,
        "dtype": str(dtype),
    }
    return cube, wavelengths, resolved_meta


def normalize01(values: np.ndarray, mask: np.ndarray | None = None) -> np.ndarray:
    values = np.asarray(values, dtype=np.float32)
    finite = np.isfinite(values)
    if mask is not None:
        finite &= mask
    if not finite.any():
        return np.zeros_like(values, dtype=np.float32)
    lo, hi = np.percentile(values[finite], [1, 99])
    if hi <= lo:
        return np.zeros_like(values, dtype=np.float32)
    return np.clip((values - lo) / (hi - lo), 0.0, 1.0).astype(np.float32)


def otsu_threshold(values: np.ndarray, bins: int = 256) -> float:
    values = np.asarray(values, dtype=np.float32)
    values = values[np.isfinite(values)]
    if values.size == 0:
        return 0.0
    lo, hi = float(values.min()), float(values.max())
    if hi <= lo:
        return lo
    hist, edges = np.histogram(values, bins=bins, range=(lo, hi))
    centers = (edges[:-1] + edges[1:]) * 0.5
    weight1 = np.cumsum(hist)
    weight2 = np.cumsum(hist[::-1])[::-1]
    mean1 = np.cumsum(hist * centers) / np.maximum(weight1, 1)
    mean2 = (np.cumsum((hist * centers)[::-1]) / np.maximum(weight2[::-1], 1))[::-1]
    variance12 = weight1[:-1] * weight2[1:] * (mean1[:-1] - mean2[1:]) ** 2
    return float(centers[int(np.argmax(variance12))])


def erode_mask(mask: np.ndarray, iterations: int = 1) -> np.ndarray:
    out = mask.astype(bool)
    for _ in range(max(iterations, 0)):
        padded = np.pad(out, 1, mode="constant", constant_values=False)
        neighbors = [
            padded[0:-2, 0:-2],
            padded[0:-2, 1:-1],
            padded[0:-2, 2:],
            padded[1:-1, 0:-2],
            padded[1:-1, 1:-1],
            padded[1:-1, 2:],
            padded[2:, 0:-2],
            padded[2:, 1:-1],
            padded[2:, 2:],
        ]
        out = np.logical_and.reduce(neighbors)
    return out


def dilate_mask(mask: np.ndarray, iterations: int = 1) -> np.ndarray:
    out = mask.astype(bool)
    for _ in range(max(iterations, 0)):
        padded = np.pad(out, 1, mode="constant", constant_values=False)
        neighbors = [
            padded[0:-2, 0:-2],
            padded[0:-2, 1:-1],
            padded[0:-2, 2:],
            padded[1:-1, 0:-2],
            padded[1:-1, 1:-1],
            padded[1:-1, 2:],
            padded[2:, 0:-2],
            padded[2:, 1:-1],
            padded[2:, 2:],
        ]
        out = np.logical_or.reduce(neighbors)
    return out


def clean_mask(mask: np.ndarray, close_iterations: int = 2) -> np.ndarray:
    return erode_mask(dilate_mask(mask, close_iterations), close_iterations)


def suppress_edge_bands(
    mask: np.ndarray,
    min_column_fraction: float = 0.5,
    edge_margin_fraction: float = 0.05,
    padding: int = 4,
) -> np.ndarray:
    """Remove tall calibration artifacts connected to either sensor edge."""
    out = mask.astype(bool, copy=True)
    if out.ndim != 2 or out.size == 0:
        return out

    height, width = out.shape
    occupied = np.flatnonzero(out.sum(axis=0) >= height * min_column_fraction)
    if occupied.size == 0:
        return out

    split_points = np.flatnonzero(np.diff(occupied) > 1)
    starts = np.r_[0, split_points + 1]
    stops = np.r_[split_points, occupied.size - 1]
    edge_margin = max(padding + 1, int(round(width * edge_margin_fraction)))

    for start_index, stop_index in zip(starts, stops):
        start = int(occupied[start_index])
        stop = int(occupied[stop_index])
        if start <= edge_margin:
            out[:, : min(width, stop + padding + 1)] = False
        elif stop >= width - edge_margin - 1:
            out[:, max(0, start - padding) :] = False
    return out


def build_sample_mask(mean_image: np.ndarray) -> np.ndarray:
    threshold = otsu_threshold(mean_image.reshape(-1))
    mask = mean_image > threshold
    # The sample is a large bright rectangle against a very dark background.
    # A gentle close removes small gaps without requiring OpenCV/scikit-image.
    mask = clean_mask(mask, close_iterations=3)
    return suppress_edge_bands(mask)


def choose_label_seed(
    signal: np.ndarray,
    sample_mask: np.ndarray,
    label_polarity: str,
    seed_percentile: float,
) -> tuple[np.ndarray, str, dict]:
    if not np.asarray(sample_mask, dtype=bool).any():
        raise ValueError("Sample mask is empty.")
    core = erode_mask(sample_mask, iterations=8)
    if core.sum() < 100:
        core = sample_mask

    values = signal[core]
    split = otsu_threshold(values)
    high = core & (signal >= split)
    low = core & (signal < split)
    high_frac = float(high.sum() / max(core.sum(), 1))
    low_frac = float(low.sum() / max(core.sum(), 1))

    if label_polarity == "auto":
        chosen = "bright" if high_frac <= low_frac else "dark"
    else:
        chosen = label_polarity

    if chosen == "bright":
        cutoff = np.percentile(values, seed_percentile)
        seed = core & (signal >= cutoff)
    else:
        cutoff = np.percentile(values, 100.0 - seed_percentile)
        seed = core & (signal <= cutoff)

    # Give the seed some spatial stability while keeping it high confidence.
    seed = clean_mask(seed, close_iterations=1)
    if seed.sum() < 50:
        seed = high if chosen == "bright" else low

    info = {
        "otsu_signal_threshold": float(split),
        "high_fraction": high_frac,
        "low_fraction": low_frac,
        "seed_pixels": int(seed.sum()),
        "sample_pixels": int(sample_mask.sum()),
        "seed_cutoff": float(cutoff),
    }
    return seed, chosen, info


def sam_degrees(cube: np.ndarray, reference: np.ndarray) -> np.ndarray:
    reference = reference.astype(np.float32)
    ref_norm = float(np.linalg.norm(reference))
    if ref_norm <= 0:
        raise ValueError("Reference spectrum norm is zero.")

    flat = cube.reshape(-1, cube.shape[2]).astype(np.float32, copy=False)
    dots = flat @ reference
    flat_norms = np.linalg.norm(flat, axis=1)
    denom = np.maximum(flat_norms * ref_norm, 1e-12)
    cosines = np.clip(dots / denom, -1.0, 1.0)
    return np.rad2deg(np.arccos(cosines)).reshape(cube.shape[:2]).astype(np.float32)


def choose_sam_mask(
    sam: np.ndarray,
    sample_mask: np.ndarray,
    threshold_deg: float | None,
    max_label_fraction: float,
) -> tuple[np.ndarray, float]:
    values = sam[sample_mask & np.isfinite(sam)]
    if values.size == 0:
        return np.zeros_like(sample_mask), 0.0

    if threshold_deg is None:
        threshold = otsu_threshold(values)
        mask = sample_mask & (sam <= threshold)
        frac = mask.sum() / max(sample_mask.sum(), 1)
        # If Otsu is too generous, use a conservative percentile. For this data,
        # the conductive path should occupy a minority of the material rectangle.
        if frac > max_label_fraction:
            threshold = float(np.percentile(values, max_label_fraction * 100.0))
            mask = sample_mask & (sam <= threshold)
    else:
        threshold = float(threshold_deg)
        mask = sample_mask & (sam <= threshold)

    mask = clean_mask(mask, close_iterations=1)
    return mask, threshold


def wavelength_index(wavelengths: np.ndarray | None, target: float, fallback: int, bands: int) -> int:
    if wavelengths is None or len(wavelengths) != bands:
        return int(np.clip(fallback, 0, bands - 1))
    return int(np.argmin(np.abs(wavelengths - target)))


def pseudo_rgb(cube: np.ndarray, wavelengths: np.ndarray | None) -> np.ndarray:
    bands = cube.shape[2]
    idx = [
        wavelength_index(wavelengths, 1650.0, int(bands * 0.85), bands),
        wavelength_index(wavelengths, 1350.0, int(bands * 0.55), bands),
        wavelength_index(wavelengths, 1100.0, int(bands * 0.25), bands),
    ]
    channels = [normalize01(cube[:, :, i]) for i in idx]
    return np.stack(channels, axis=2)


def apply_colormap(values: np.ndarray) -> np.ndarray:
    palette = np.array(
        [
            [68, 1, 84],
            [59, 82, 139],
            [33, 145, 140],
            [94, 201, 98],
            [253, 231, 37],
        ],
        dtype=np.float32,
    )
    x = np.clip(values.astype(np.float32), 0.0, 1.0)
    pos = x * (len(palette) - 1)
    lo = np.floor(pos).astype(np.int32)
    hi = np.clip(lo + 1, 0, len(palette) - 1)
    mix = (pos - lo)[..., None]
    return ((1.0 - mix) * palette[lo] + mix * palette[hi]).astype(np.uint8)


def mask_to_yellow(mask: np.ndarray) -> np.ndarray:
    image = np.zeros((*mask.shape, 3), dtype=np.uint8)
    image[:, :] = [68, 1, 84]
    image[mask] = [253, 231, 37]
    return image


def overlay_mask(rgb: np.ndarray, mask: np.ndarray, alpha: float = 0.55) -> np.ndarray:
    base = np.clip(rgb * 255.0, 0, 255).astype(np.uint8)
    color = np.array([255, 230, 40], dtype=np.float32)
    out = base.astype(np.float32)
    out[mask] = (1.0 - alpha) * out[mask] + alpha * color
    return np.clip(out, 0, 255).astype(np.uint8)


def write_png(path: Path, image: np.ndarray) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    image = np.asarray(image)
    if image.dtype != np.uint8:
        image = np.clip(image, 0, 255).astype(np.uint8)

    if image.ndim == 2:
        color_type = 0
        rows = image
    elif image.ndim == 3 and image.shape[2] == 3:
        color_type = 2
        rows = image.reshape(image.shape[0], image.shape[1] * 3)
    else:
        raise ValueError(f"PNG image must be HxW or HxWx3 uint8, got shape {image.shape}")

    height, width = image.shape[:2]
    raw = b"".join(b"\x00" + rows[y].tobytes() for y in range(height))

    def chunk(tag: bytes, payload: bytes) -> bytes:
        return (
            struct.pack(">I", len(payload))
            + tag
            + payload
            + struct.pack(">I", zlib.crc32(tag + payload) & 0xFFFFFFFF)
        )

    png = (
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, color_type, 0, 0, 0))
        + chunk(b"IDAT", zlib.compress(raw, level=6))
        + chunk(b"IEND", b"")
    )
    path.write_bytes(png)


def write_report(
    output_dir: Path,
    stem: str,
    meta: dict,
    chosen_polarity: str,
    seed_info: dict,
    sam_threshold: float,
    mask_fraction: float,
) -> None:
    report = f"""<!doctype html>
<html lang="zh-Hant">
<head>
  <meta charset="utf-8">
  <title>{html.escape(stem)} SAM mask</title>
  <style>
    body {{ font-family: Arial, "Microsoft JhengHei", sans-serif; margin: 24px; color: #222; }}
    .grid {{ display: grid; grid-template-columns: repeat(2, minmax(260px, 1fr)); gap: 20px; }}
    figure {{ margin: 0; }}
    img {{ max-width: 100%; border: 1px solid #ddd; }}
    table {{ border-collapse: collapse; margin: 16px 0 24px; }}
    td, th {{ border: 1px solid #ddd; padding: 6px 10px; text-align: left; }}
  </style>
</head>
<body>
  <h1>{html.escape(stem)} SAM mask</h1>
  <table>
    <tr><th>lines</th><td>{meta["lines"]}</td></tr>
    <tr><th>samples</th><td>{meta["samples"]}</td></tr>
    <tr><th>bands after trim</th><td>{meta["bands"]}</td></tr>
    <tr><th>label polarity</th><td>{chosen_polarity}</td></tr>
    <tr><th>SAM threshold</th><td>{sam_threshold:.4f} deg</td></tr>
    <tr><th>label mask fraction of sample</th><td>{mask_fraction:.4%}</td></tr>
    <tr><th>seed pixels</th><td>{seed_info["seed_pixels"]}</td></tr>
  </table>
  <div class="grid">
    <figure><img src="pseudo_rgb.png"><figcaption>pseudo_rgb.png</figcaption></figure>
    <figure><img src="sam_score.png"><figcaption>sam_score.png</figcaption></figure>
    <figure><img src="sam_mask.png"><figcaption>sam_mask.png</figcaption></figure>
    <figure><img src="sam_overlay.png"><figcaption>sam_overlay.png</figcaption></figure>
  </div>
</body>
</html>
"""
    (output_dir / "report.html").write_text(report, encoding="utf-8")


def resolve_data_dir(script_dir: Path, user_data_dir: str | None) -> Path:
    candidates = []
    if user_data_dir:
        candidates.append(Path(user_data_dir))
    candidates.append(script_dir / "導電材料_RT")
    candidates.append(script_dir.parent / "20260528_導電材料" / "導電材料_RT")

    for candidate in candidates:
        if candidate.exists():
            return candidate.resolve()

    raise FileNotFoundError(
        "Cannot find data directory. Expected one of:\n"
        + "\n".join(f"  - {c}" for c in candidates)
    )


def process_one(hdr_path: Path, output_root: Path, args: argparse.Namespace) -> None:
    print(f"[load] {hdr_path}")
    cube, wavelengths, meta = load_envi_bil(hdr_path, trim_bands=args.trim_bands)

    mean_image = cube.mean(axis=2)
    sample_mask = build_sample_mask(mean_image)
    signal = normalize01(mean_image, sample_mask)
    seed, polarity, seed_info = choose_label_seed(
        signal=signal,
        sample_mask=sample_mask,
        label_polarity=args.label_polarity,
        seed_percentile=args.seed_percentile,
    )

    if seed.sum() == 0:
        raise ValueError(f"{hdr_path.name}: automatic label seed is empty.")

    reference = cube[seed].mean(axis=0)
    sam = sam_degrees(cube, reference)
    label_mask, sam_threshold = choose_sam_mask(
        sam=sam,
        sample_mask=sample_mask,
        threshold_deg=args.sam_threshold_deg,
        max_label_fraction=args.max_label_fraction,
    )

    output_dir = output_root / hdr_path.stem
    output_dir.mkdir(parents=True, exist_ok=True)

    rgb = pseudo_rgb(cube, wavelengths)
    sam_norm = 1.0 - normalize01(sam, sample_mask)

    write_png(output_dir / "pseudo_rgb.png", (rgb * 255).astype(np.uint8))
    write_png(output_dir / "sam_score.png", apply_colormap(sam_norm))
    write_png(output_dir / "sam_mask.png", mask_to_yellow(label_mask))
    write_png(output_dir / "sam_overlay.png", overlay_mask(rgb, label_mask))
    np.save(output_dir / "sam_mask.npy", label_mask.astype(np.uint8))

    mask_fraction = float(label_mask.sum() / max(sample_mask.sum(), 1))
    write_report(
        output_dir=output_dir,
        stem=hdr_path.stem,
        meta=meta,
        chosen_polarity=polarity,
        seed_info=seed_info,
        sam_threshold=sam_threshold,
        mask_fraction=mask_fraction,
    )

    print(f"[done] {hdr_path.stem}")
    print(f"       output: {output_dir}")
    print(f"       label polarity: {polarity}")
    print(f"       SAM threshold: {sam_threshold:.4f} deg")
    print(f"       mask/sample fraction: {mask_fraction:.4%}")


def list_headers(data_dir: Path, include_excluded: bool) -> list[Path]:
    headers = sorted(data_dir.glob("*_RT.hdr"))
    if include_excluded:
        return headers
    return [p for p in headers if p.stem not in EXCLUDED_FOR_TRAINING]


def build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Automatically generate SAM label masks from corrected FX17 ENVI HDR/RAW files."
    )
    parser.add_argument("--data-dir", default=None, help="Folder containing *_RT.hdr and *_RT.raw files.")
    parser.add_argument("--file", default="50u_1_RT.hdr", help="Single HDR filename to process.")
    parser.add_argument("--all", action="store_true", help="Process every non-excluded *_RT.hdr file.")
    parser.add_argument("--include-excluded", action="store_true", help="Include files with known HDR/RAW line mismatch.")
    parser.add_argument("--output-dir", default="sam_mask_outputs", help="Output folder under CAE training folder.")
    parser.add_argument("--trim-bands", type=int, default=10, help="Bands to trim from each spectral end.")
    parser.add_argument(
        "--label-polarity",
        choices=["auto", "bright", "dark"],
        default="auto",
        help="Whether the conductive label is brighter or darker than the substrate in mean reflectance.",
    )
    parser.add_argument("--seed-percentile", type=float, default=85.0, help="High-confidence seed percentile.")
    parser.add_argument("--sam-threshold-deg", type=float, default=None, help="Manual SAM threshold in degrees.")
    parser.add_argument(
        "--max-label-fraction",
        type=float,
        default=0.32,
        help="Max label area fraction inside sample when auto-thresholding SAM.",
    )
    return parser


def main() -> None:
    script_dir = Path(__file__).resolve().parent
    args = build_arg_parser().parse_args()

    data_dir = resolve_data_dir(script_dir, args.data_dir)
    output_root = (script_dir / args.output_dir).resolve()
    output_root.mkdir(parents=True, exist_ok=True)

    print(f"[data] {data_dir}")
    print(f"[output] {output_root}")
    print(f"[excluded by default] {', '.join(sorted(EXCLUDED_FOR_TRAINING))}")

    if args.all:
        hdr_paths = list_headers(data_dir, include_excluded=args.include_excluded)
    else:
        hdr_paths = [data_dir / args.file]

    for hdr_path in hdr_paths:
        if not hdr_path.exists():
            raise FileNotFoundError(f"Cannot find HDR file: {hdr_path}")
        if hdr_path.stem in EXCLUDED_FOR_TRAINING and not args.include_excluded:
            print(f"[skip] {hdr_path.name}: known HDR/RAW line mismatch; pass --include-excluded to process anyway.")
            continue
        process_one(hdr_path, output_root, args)


if __name__ == "__main__":
    main()
