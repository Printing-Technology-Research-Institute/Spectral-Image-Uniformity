from __future__ import annotations

import json
import os
import re
import tempfile
from datetime import datetime
from pathlib import Path
from typing import Any

import numpy as np


ENVI_FLOAT32 = 4
ENVI_UINT16 = 12


def _json_safe(value: Any) -> Any:
    if isinstance(value, dict):
        return {str(key): _json_safe(item) for key, item in value.items()}
    if isinstance(value, (list, tuple)):
        return [_json_safe(item) for item in value]
    if isinstance(value, Path):
        return str(value)
    if isinstance(value, np.ndarray):
        return value.tolist()
    if isinstance(value, np.generic):
        return value.item()
    if isinstance(value, (str, int, float, bool)) or value is None:
        return value
    return str(value)


def read_envi_wavelengths(
    hdr_path: str | Path,
    *,
    trim_bands: int = 0,
    expected_bands: int | None = None,
) -> list[float] | None:
    """Read and optionally trim an ENVI wavelength list."""
    path = Path(hdr_path)
    if not path.is_file():
        return None
    text = path.read_text(encoding="utf-8", errors="ignore")
    match = re.search(r"(?is)wavelength\s*=\s*\{(?P<body>.*?)\}", text)
    if match is None:
        return None
    values = [
        float(item.group(0))
        for item in re.finditer(
            r"[-+]?\d+(?:\.\d+)?(?:[eE][-+]?\d+)?",
            match.group("body"),
        )
    ]
    trim = max(0, int(trim_bands))
    if trim:
        if len(values) <= 2 * trim:
            return None
        values = values[trim:-trim]
    if expected_bands is not None and len(values) != int(expected_bands):
        return None
    return values


def _format_envi_list(values: list[float]) -> str:
    rows: list[str] = []
    for start in range(0, len(values), 8):
        rows.append(", ".join(f"{value:.6f}" for value in values[start : start + 8]))
    return ",\n  ".join(rows)


def build_envi_header(
    *,
    raw_name: str,
    lines: int,
    samples: int,
    bands: int,
    wavelengths_nm: list[float] | None = None,
    sensor_type: str = "Specim FX17e",
    description: str = "FX17 black-white corrected reflectance sample cube",
    data_type: int = ENVI_FLOAT32,
    reflectance_scale_factor: float | None = 1.0,
) -> str:
    header = [
        "ENVI",
        f"description = {{{description}}}",
        f"samples = {int(samples)}",
        f"lines = {int(lines)}",
        f"bands = {int(bands)}",
        "header offset = 0",
        "file type = ENVI Standard",
        f"data type = {int(data_type)}",
        "interleave = bil",
        "byte order = 0",
        f"data file = {{{raw_name}}}",
        f"sensor type = {{{sensor_type}}}",
        "wavelength units = Nanometers",
    ]
    if reflectance_scale_factor is not None:
        header.append(
            f"reflectance scale factor = {float(reflectance_scale_factor):g}"
        )
    if wavelengths_nm is not None and len(wavelengths_nm) == int(bands):
        header.append(f"wavelength = {{\n  {_format_envi_list(wavelengths_nm)}\n}}")
    return "\n".join(header) + "\n"


def _temporary_path(directory: Path, suffix: str) -> Path:
    handle = tempfile.NamedTemporaryFile(
        prefix=".fx17_",
        suffix=suffix,
        dir=directory,
        delete=False,
    )
    path = Path(handle.name)
    handle.close()
    return path


def _write_json_atomic(path: Path, payload: dict[str, Any]) -> None:
    temporary = _temporary_path(path.parent, ".json.tmp")
    try:
        temporary.write_text(
            json.dumps(_json_safe(payload), ensure_ascii=False, indent=2) + "\n",
            encoding="utf-8",
        )
        os.replace(temporary, path)
    finally:
        temporary.unlink(missing_ok=True)


def write_sample_record(
    directory: str | Path,
    *,
    stem_name: str,
    sample_id: str,
    cube: np.ndarray,
    acquisition_metadata: dict[str, Any] | None = None,
    wavelengths_nm: list[float] | None = None,
    sensor_type: str = "Specim FX17e",
    data_kind: str = "calibrated",
    paired_record: dict[str, Any] | None = None,
) -> Path:
    """Atomically write one calibrated or uncalibrated ENVI sample record."""
    output_dir = Path(directory).resolve()
    output_dir.mkdir(parents=True, exist_ok=True)
    cube = np.asarray(cube)
    if cube.ndim != 3:
        raise ValueError(f"Expected cube shape (lines, samples, bands), got {cube.shape}")
    if data_kind == "calibrated":
        stored_cube = np.asarray(cube, dtype=np.float32)
        if not np.isfinite(stored_cube).all():
            raise ValueError("Cannot save a sample cube containing non-finite values")
        storage_dtype = np.dtype("<f4")
        envi_data_type = ENVI_FLOAT32
        dtype_name = "float32"
        content = "black-white corrected model-input reflectance cube"
        description = "FX17 black-white corrected reflectance sample cube"
        reflectance_scale_factor: float | None = 1.0
    elif data_kind == "uncalibrated":
        if not np.issubdtype(cube.dtype, np.integer):
            raise TypeError("Uncalibrated DN cube must use an integer dtype")
        if cube.size and (int(cube.min()) < 0 or int(cube.max()) > 65535):
            raise ValueError("Uncalibrated DN values must fit in uint16")
        stored_cube = np.asarray(cube, dtype=np.uint16)
        storage_dtype = np.dtype("<u2")
        envi_data_type = ENVI_UINT16
        dtype_name = "uint16"
        content = "uncalibrated decoded camera DN cube"
        description = "FX17 uncalibrated decoded camera DN sample cube"
        reflectance_scale_factor = None
    else:
        raise ValueError("data_kind must be 'calibrated' or 'uncalibrated'")

    lines, samples, bands = (int(value) for value in stored_cube.shape)
    stem = output_dir / stem_name
    raw_path = stem.with_suffix(".raw")
    hdr_path = stem.with_suffix(".hdr")
    json_path = stem.with_suffix(".json")
    if any(path.exists() for path in (raw_path, hdr_path, json_path)):
        raise FileExistsError(f"Sample record already exists: {stem}")

    raw_temporary = _temporary_path(output_dir, ".raw.tmp")
    hdr_temporary = _temporary_path(output_dir, ".hdr.tmp")
    try:
        with raw_temporary.open("wb") as handle:
            # ENVI BIL is (line, band, sample); stream blocks to avoid a second
            # full-size transposed cube in memory.
            for start in range(0, lines, 32):
                bil = np.ascontiguousarray(
                    stored_cube[start : start + 32].transpose(0, 2, 1),
                    dtype=storage_dtype,
                )
                bil.tofile(handle)
        expected_bytes = lines * samples * bands * storage_dtype.itemsize
        if raw_temporary.stat().st_size != expected_bytes:
            raise IOError(
                f"RAW byte count mismatch: expected {expected_bytes}, "
                f"got {raw_temporary.stat().st_size}"
            )

        hdr_temporary.write_text(
            build_envi_header(
                raw_name=raw_path.name,
                lines=lines,
                samples=samples,
                bands=bands,
                wavelengths_nm=wavelengths_nm,
                sensor_type=sensor_type,
                description=description,
                data_type=envi_data_type,
                reflectance_scale_factor=reflectance_scale_factor,
            ),
            encoding="utf-8",
        )
        os.replace(raw_temporary, raw_path)
        os.replace(hdr_temporary, hdr_path)

        recorded_at = datetime.now().astimezone().isoformat(timespec="seconds")
        metadata = {
            "schema_version": 2,
            "sample_id": sample_id,
            "recorded_at": recorded_at,
            "status": "acquired",
            "data_kind": data_kind,
            "content": content,
            "files": {
                "raw": raw_path.name,
                "hdr": hdr_path.name,
                "parameters": json_path.name,
            },
            "envi": {
                "shape": [lines, samples, bands],
                "axis_order_in_application": [
                    "scan_line",
                    "spatial_pixel",
                    "spectral_band",
                ],
                "interleave_on_disk": "bil",
                "bil_axis_order": ["scan_line", "spectral_band", "spatial_pixel"],
                "dtype": dtype_name,
                "data_type": envi_data_type,
                "byte_order": 0,
                "header_offset": 0,
                "wavelength_count": 0 if wavelengths_nm is None else len(wavelengths_nm),
                "raw_bytes": expected_bytes,
            },
            "paired_record": paired_record or {},
            "parameters": acquisition_metadata or {},
            "analysis": {"status": "pending"},
        }
        _write_json_atomic(json_path, metadata)
        return stem
    except Exception:
        raw_temporary.unlink(missing_ok=True)
        hdr_temporary.unlink(missing_ok=True)
        raw_path.unlink(missing_ok=True)
        hdr_path.unlink(missing_ok=True)
        json_path.unlink(missing_ok=True)
        raise


def write_sample_record_pair(
    root_directory: str | Path,
    *,
    stem_name: str,
    sample_id: str,
    uncalibrated_cube: np.ndarray,
    calibrated_cube: np.ndarray,
    acquisition_metadata: dict[str, Any] | None = None,
    uncalibrated_wavelengths_nm: list[float] | None = None,
    calibrated_wavelengths_nm: list[float] | None = None,
    sensor_type: str = "Specim FX17e",
) -> tuple[Path, Path]:
    """Write a same-stem uncalibrated/calibrated record pair."""
    root = Path(root_directory).resolve()
    uncalibrated_dir = root / "uncalibrated"
    calibrated_dir = root / "calibrated"
    uncalibrated_stem = uncalibrated_dir / stem_name
    calibrated_stem = calibrated_dir / stem_name

    def record_links(stem: Path, role: str) -> dict[str, Any]:
        return {
            "role": role,
            "raw": str(stem.with_suffix(".raw")),
            "hdr": str(stem.with_suffix(".hdr")),
            "parameters": str(stem.with_suffix(".json")),
        }

    written: list[Path] = []
    try:
        uncalibrated = write_sample_record(
            uncalibrated_dir,
            stem_name=stem_name,
            sample_id=sample_id,
            cube=uncalibrated_cube,
            acquisition_metadata=acquisition_metadata,
            wavelengths_nm=uncalibrated_wavelengths_nm,
            sensor_type=sensor_type,
            data_kind="uncalibrated",
            paired_record=record_links(calibrated_stem, "calibrated"),
        )
        written.append(uncalibrated)
        calibrated = write_sample_record(
            calibrated_dir,
            stem_name=stem_name,
            sample_id=sample_id,
            cube=calibrated_cube,
            acquisition_metadata=acquisition_metadata,
            wavelengths_nm=calibrated_wavelengths_nm,
            sensor_type=sensor_type,
            data_kind="calibrated",
            paired_record=record_links(uncalibrated_stem, "uncalibrated"),
        )
        written.append(calibrated)
        return uncalibrated, calibrated
    except Exception:
        for stem in (uncalibrated_stem, calibrated_stem):
            for suffix in (".raw", ".hdr", ".json"):
                stem.with_suffix(suffix).unlink(missing_ok=True)
        raise


def write_sample_record_bundle(
    sample_directory: str | Path,
    *,
    file_stem: str,
    sample_id: str,
    uncalibrated_cube: np.ndarray,
    calibrated_cube: np.ndarray,
    acquisition_metadata: dict[str, Any] | None = None,
    uncalibrated_wavelengths_nm: list[float] | None = None,
    calibrated_wavelengths_nm: list[float] | None = None,
    sensor_type: str = "Specim FX17e",
) -> tuple[Path, Path]:
    """Write both ENVI records into one sample folder with one capture-time stem."""
    output_dir = Path(sample_directory).resolve()
    output_dir.mkdir(parents=True, exist_ok=True)
    uncalibrated_name = f"{file_stem}_未校正"
    calibrated_name = f"{file_stem}_校正"
    uncalibrated_stem = output_dir / uncalibrated_name
    calibrated_stem = output_dir / calibrated_name

    def record_links(stem: Path, role: str) -> dict[str, Any]:
        return {
            "role": role,
            "raw": str(stem.with_suffix(".raw")),
            "hdr": str(stem.with_suffix(".hdr")),
            "parameters": str(stem.with_suffix(".json")),
        }

    try:
        uncalibrated = write_sample_record(
            output_dir,
            stem_name=uncalibrated_name,
            sample_id=sample_id,
            cube=uncalibrated_cube,
            acquisition_metadata=acquisition_metadata,
            wavelengths_nm=uncalibrated_wavelengths_nm,
            sensor_type=sensor_type,
            data_kind="uncalibrated",
            paired_record=record_links(calibrated_stem, "calibrated"),
        )
        calibrated = write_sample_record(
            output_dir,
            stem_name=calibrated_name,
            sample_id=sample_id,
            cube=calibrated_cube,
            acquisition_metadata=acquisition_metadata,
            wavelengths_nm=calibrated_wavelengths_nm,
            sensor_type=sensor_type,
            data_kind="calibrated",
            paired_record=record_links(uncalibrated_stem, "uncalibrated"),
        )
        return uncalibrated, calibrated
    except Exception:
        for stem in (uncalibrated_stem, calibrated_stem):
            for suffix in (".raw", ".hdr", ".json"):
                stem.with_suffix(suffix).unlink(missing_ok=True)
        raise


def update_sample_record(
    stem: str | Path,
    updates: dict[str, Any],
) -> None:
    metadata_path = Path(stem).with_suffix(".json")
    metadata = json.loads(metadata_path.read_text(encoding="utf-8"))
    metadata.update(_json_safe(updates))
    _write_json_atomic(metadata_path, metadata)
