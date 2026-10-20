from __future__ import annotations

import argparse
import csv
import json
import re
import time
from dataclasses import asdict, dataclass
from pathlib import Path

import numpy as np

from live_analysis import LiveAnalysisResult, LiveAnalysisWorker
from replay_backend import ENVI_DTYPE, _header_value
from sample_boundary_detector import (
    BoundaryEvent,
    DetectorConfig,
    LineCalibrator,
    SampleBoundaryDetector,
)


APP_DIR = Path(__file__).resolve().parent
WORKSPACE = APP_DIR.parents[1]
DEFAULT_PROVENANCE = (
    WORKSPACE / "CAE訓練" / "導電材料_RT_v3" / "calibration_provenance.json"
)
DEFAULT_OUTPUT_DIR = APP_DIR / "stream_detection_outputs"


@dataclass(frozen=True)
class RawSource:
    sample_id: str
    hdr_path: Path
    raw_path: Path
    lines: int
    bands: int
    width: int
    header_offset: int
    dtype: np.dtype


def natural_key(sample_id: str) -> tuple[int, int]:
    match = re.match(r"(\d+)u_(\d+)", sample_id, flags=re.IGNORECASE)
    return (int(match.group(1)), int(match.group(2))) if match else (999, 999)


def resolve_workspace_file(
    stored_path: str | Path,
    local_directory: str | Path,
) -> Path:
    """Resolve a provenance path after the workspace moves to another PC."""
    original = Path(stored_path)
    if original.is_file():
        return original
    relocated = Path(local_directory) / original.name
    if relocated.is_file():
        return relocated
    raise FileNotFoundError(
        f"File not found at the recorded or relocated path: "
        f"{original} ; {relocated}"
    )


def source_from_record(
    record: dict,
    *,
    source_directory: str | Path = WORKSPACE / "20260528_導電材料",
) -> RawSource:
    hdr_path = resolve_workspace_file(record["source_hdr"], source_directory)
    raw_path = resolve_workspace_file(record["source_raw"], source_directory)
    text = hdr_path.read_text(encoding="utf-8", errors="ignore")
    width = _header_value(text, "samples")
    bands = _header_value(text, "bands")
    header_offset = _header_value(text, "header offset", 0)
    dtype_code = _header_value(text, "data type")
    byte_order = _header_value(text, "byte order", 0)
    if dtype_code not in ENVI_DTYPE:
        raise ValueError(f"Unsupported ENVI data type {dtype_code}: {hdr_path}")
    dtype = np.dtype(ENVI_DTYPE[dtype_code]).newbyteorder(
        "<" if byte_order == 0 else ">"
    )
    bytes_per_line = bands * width * dtype.itemsize
    payload_bytes = raw_path.stat().st_size - header_offset
    if payload_bytes % bytes_per_line:
        raise ValueError(f"RAW byte count is not divisible by one line: {raw_path}")
    lines = payload_bytes // bytes_per_line
    sample_id = f"{hdr_path.stem}_RT"
    return RawSource(
        sample_id=sample_id,
        hdr_path=hdr_path,
        raw_path=raw_path,
        lines=int(lines),
        bands=bands,
        width=width,
        header_offset=header_offset,
        dtype=dtype,
    )


def load_sources(
    provenance: dict,
    *,
    source_directory: str | Path = WORKSPACE / "20260528_導電材料",
) -> list[RawSource]:
    sources = [
        source_from_record(record, source_directory=source_directory)
        for record in provenance["records"]
    ]
    sources.sort(key=lambda source: natural_key(source.sample_id))
    if len(sources) != int(provenance["sample_count"]):
        raise ValueError("Provenance sample count does not match its records")
    return sources


def open_raw(source: RawSource) -> np.memmap:
    return np.memmap(
        source.raw_path,
        dtype=source.dtype,
        mode="r",
        offset=source.header_offset,
        shape=(source.lines, source.bands, source.width),
    )


def event_record(event: BoundaryEvent) -> dict:
    cube_shape = None
    cube_probe_finite = None
    if event.cube is not None:
        cube_shape = [int(value) for value in event.cube.shape]
        line_step = max(1, event.cube.shape[0] // 16)
        probe = event.cube[::line_step, ::16, ::8]
        cube_probe_finite = bool(np.isfinite(probe).all())
    record = {
        "sample_start_line": event.sample_start_line,
        "sample_end_line": event.sample_end_line,
        "sample_line_count": event.sample_line_count,
        "event_stream_line": event.stream_line,
        "reason": event.reason,
        "cube_shape": cube_shape,
        "cube_probe_finite": cube_probe_finite,
    }
    event.cube = None
    return record


def analysis_record(result: LiveAnalysisResult) -> dict:
    return {
        "live_analysis_status": "ok",
        "live_uniformity": result.uniformity,
        "live_resistance_ohm": result.resistance_ohm,
        "live_mean_residual": result.mean_residual,
        "live_p95_residual": result.p95_residual,
        "live_hot_area_ratio": result.hot_area_ratio,
        "live_label_pixels": result.label_pixels,
        "live_analysis_seconds": result.elapsed_seconds,
        "live_analysis_device": result.device,
    }


def write_csv(path: Path, rows: list[dict]) -> None:
    if not rows:
        return
    with path.open("w", newline="", encoding="utf-8-sig") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)


def save_score_plot(
    path: Path,
    scores: np.ndarray,
    truth_rows: list[dict],
    config: DetectorConfig,
) -> bool:
    try:
        import matplotlib

        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError:
        return False

    figure, axis = plt.subplots(figsize=(16, 6))
    axis.plot(scores, color="#334e68", linewidth=0.7, label="Material score")
    axis.axhline(
        config.start_threshold,
        color="#087f5b",
        linestyle="--",
        linewidth=1.2,
        label="Start threshold",
    )
    axis.axhline(
        config.end_threshold,
        color="#c92a2a",
        linestyle=":",
        linewidth=1.2,
        label="End threshold",
    )
    for index, row in enumerate(truth_rows):
        axis.axvspan(
            row["truth_start_line"],
            row["truth_end_line"],
            color="#74c0fc" if index % 2 == 0 else "#ffd43b",
            alpha=0.10,
            linewidth=0,
        )
    axis.set_xlabel("Continuous stream line")
    axis.set_ylabel("Material fraction")
    axis.set_ylim(-0.02, 0.82)
    axis.set_title("Automatic sample boundary detection on 30 concatenated RAW captures")
    axis.grid(True, axis="y", alpha=0.2)
    axis.legend(loc="upper right")
    figure.tight_layout()
    figure.savefig(path, dpi=170)
    plt.close(figure)
    return True


def run_simulation(args: argparse.Namespace) -> dict:
    provenance_path = Path(args.provenance).resolve()
    provenance = json.loads(provenance_path.read_text(encoding="utf-8"))
    sources = load_sources(provenance)
    first = sources[0]
    if any(
        source.bands != first.bands or source.width != first.width
        for source in sources
    ):
        raise ValueError("Every source must share the same band and spatial dimensions")

    config = DetectorConfig(
        material_pixel_threshold=args.material_pixel_threshold,
        start_threshold=args.start_threshold,
        end_threshold=args.end_threshold,
        start_confirm_lines=args.start_confirm_lines,
        end_confirm_lines=args.end_confirm_lines,
        min_sample_lines=args.min_sample_lines,
        max_sample_lines=args.max_sample_lines,
    )
    reference_directory = WORKSPACE / "20260528_導電材料"
    dark_reference = resolve_workspace_file(
        provenance["dark_reference"],
        reference_directory,
    )
    white_reference = resolve_workspace_file(
        provenance["white_reference"],
        reference_directory,
    )
    calibrator = LineCalibrator.from_files(
        dark_reference,
        white_reference,
        bands=first.bands,
        width=first.width,
        trim_bands=args.trim_bands,
        clip_max=args.clip_max,
        min_white_column_fraction=args.min_white_column_fraction,
        material_pixel_threshold=config.material_pixel_threshold,
    )
    detector = SampleBoundaryDetector(config, collect_cube=args.collect_cubes)
    analysis_worker = None
    analysis_results: dict[str, dict] = {}
    analysis_errors: dict[str, str] = {}
    analysis_submitted = 0
    if args.analyze:
        if not args.collect_cubes:
            raise ValueError("--analyze requires --collect-cubes")
        analysis_worker = LiveAnalysisWorker(
            device=args.analysis_device,
            max_pending=1,
        )
        deadline = time.monotonic() + args.analysis_timeout_seconds
        while True:
            messages = analysis_worker.poll()
            ready = any(message.kind == "ready" for message in messages)
            initialization_error = next(
                (
                    message.error
                    for message in messages
                    if message.kind == "error" and message.sample_id is None
                ),
                None,
            )
            if initialization_error:
                analysis_worker.close()
                raise RuntimeError(initialization_error)
            if ready:
                break
            if time.monotonic() >= deadline:
                analysis_worker.close()
                raise TimeoutError("Timed out while loading the live analysis models")
            time.sleep(0.05)

    truth_rows: list[dict] = []
    detections: list[dict] = []
    rejected: list[dict] = []
    score_parts: list[np.ndarray] = []
    stream_line = 0
    start_time = time.perf_counter()

    def collect_analysis_messages() -> None:
        if analysis_worker is None:
            return
        for message in analysis_worker.poll():
            if message.kind == "result" and message.result is not None:
                analysis_results[message.result.sample_id] = analysis_record(
                    message.result
                )
            elif message.kind == "error" and message.sample_id is not None:
                analysis_errors[message.sample_id] = message.error or "Unknown error"

    def accept_events(events: list[BoundaryEvent]) -> None:
        nonlocal analysis_submitted
        for event in events:
            if event.kind == "sample_end":
                detection_index = len(detections)
                sample_id = (
                    sources[detection_index].sample_id
                    if detection_index < len(sources)
                    else f"detected_{detection_index + 1}"
                )
                if analysis_worker is not None:
                    if event.cube is None:
                        raise RuntimeError("Detector did not return a model-ready cube")
                    submitted = analysis_worker.submit_cube(
                        sample_id,
                        event.cube,
                        block=True,
                    )
                    if not submitted:
                        raise RuntimeError(f"Could not queue {sample_id} for analysis")
                    analysis_submitted += 1
                detection = event_record(event)
                detection["detected_sample_id"] = sample_id
                detections.append(detection)
                collect_analysis_messages()
            elif event.kind == "sample_rejected":
                rejected.append(event_record(event))

    for source_index, source in enumerate(sources):
        raw = open_raw(source)
        local_scores: list[np.ndarray] = []
        background_lines: list[np.ndarray] = []
        source_stream_start = stream_line

        for start in range(0, source.lines, args.chunk_lines):
            stop = min(start + args.chunk_lines, source.lines)
            raw_block = np.asarray(raw[start:stop])
            corrected = calibrator.calibrate_block(raw_block)
            scores = calibrator.line_scores(corrected)
            local_scores.append(scores)
            score_parts.append(scores)

            for offset, score in enumerate(scores):
                local_index = start + offset
                if (
                    score <= config.end_threshold
                    and len(background_lines) < args.gap_lines
                ):
                    background_lines.append(np.asarray(raw_block[offset]).copy())
                payload = (
                    calibrator.model_line(corrected[offset])
                    if args.collect_cubes
                    else None
                )
                accept_events(
                    detector.feed(
                        float(score),
                        payload,
                        stream_line=stream_line + local_index,
                    )
                )
        source_scores = np.concatenate(local_scores)
        start_candidates = np.flatnonzero(source_scores >= config.start_threshold)
        if start_candidates.size == 0:
            raise RuntimeError(f"No material region found in {source.sample_id}")
        truth_start = int(start_candidates[0])
        end_candidates = np.flatnonzero(
            (np.arange(source_scores.size) >= truth_start)
            & (source_scores > config.end_threshold)
        )
        truth_end = int(end_candidates[-1])
        reference_region = np.arange(truth_start, truth_end + 1)
        if np.any(source_scores[reference_region] <= config.end_threshold):
            raise RuntimeError(f"Material region is not contiguous in {source.sample_id}")
        truth_rows.append(
            {
                "sample_id": source.sample_id,
                "source_hdr": str(source.hdr_path.resolve()),
                "source_raw": str(source.raw_path.resolve()),
                "source_lines": source.lines,
                "source_stream_start": source_stream_start,
                "truth_start_line": source_stream_start + truth_start,
                "truth_end_line": source_stream_start + truth_end,
                "truth_line_count": truth_end - truth_start + 1,
                "source_score_min": float(source_scores.min()),
                "source_score_max": float(source_scores.max()),
            }
        )
        stream_line += source.lines
        del raw

        if background_lines:
            gap_raw = np.stack(
                [background_lines[index % len(background_lines)] for index in range(args.gap_lines)]
            )
        else:
            dark_raw = np.rint(calibrator.dark).astype(source.dtype)
            gap_raw = np.repeat(dark_raw[None, :, :], args.gap_lines, axis=0)
        corrected_gap = calibrator.calibrate_block(gap_raw)
        gap_scores = calibrator.line_scores(corrected_gap)
        score_parts.append(gap_scores)
        for offset, score in enumerate(gap_scores):
            payload = (
                calibrator.model_line(corrected_gap[offset])
                if args.collect_cubes
                else None
            )
            accept_events(
                detector.feed(
                    float(score),
                    payload,
                    stream_line=stream_line + offset,
                )
            )
        stream_line += args.gap_lines
        print(
            f"[{source_index + 1:02d}/{len(sources)}] {source.sample_id}: "
            f"truth={truth_start}..{truth_end} score_max={source_scores.max():.3f}"
        )

    accept_events(detector.flush())
    if analysis_worker is not None:
        deadline = time.monotonic() + args.analysis_timeout_seconds
        while len(analysis_results) + len(analysis_errors) < analysis_submitted:
            collect_analysis_messages()
            if time.monotonic() >= deadline:
                break
            time.sleep(0.05)
        collect_analysis_messages()
        analysis_worker.close()
    all_scores = np.concatenate(score_parts)
    elapsed = time.perf_counter() - start_time

    rows: list[dict] = []
    for index, truth in enumerate(truth_rows):
        detected = detections[index] if index < len(detections) else {}
        detected_start = detected.get("sample_start_line")
        detected_end = detected.get("sample_end_line")
        start_error = (
            int(detected_start) - truth["truth_start_line"]
            if detected_start is not None
            else None
        )
        end_error = (
            int(detected_end) - truth["truth_end_line"]
            if detected_end is not None
            else None
        )
        rows.append(
            {
                **truth,
                "detected_start_line": detected_start,
                "detected_end_line": detected_end,
                "detected_line_count": detected.get("sample_line_count"),
                "start_error_lines": start_error,
                "end_error_lines": end_error,
                "line_count_error": (
                    int(detected["sample_line_count"]) - truth["truth_line_count"]
                    if detected.get("sample_line_count") is not None
                    else None
                ),
                "end_reason": detected.get("reason"),
                "cube_shape": detected.get("cube_shape"),
                "cube_probe_finite": detected.get("cube_probe_finite"),
                **analysis_results.get(truth["sample_id"], {}),
                **(
                    {
                        "live_analysis_status": "error",
                        "live_analysis_error": analysis_errors[truth["sample_id"]],
                    }
                    if truth["sample_id"] in analysis_errors
                    else {}
                ),
            }
        )

    errors = [
        abs(int(row[key]))
        for row in rows
        for key in ("start_error_lines", "end_error_lines")
        if row[key] is not None
    ]
    passed = (
        len(detections) == len(sources)
        and not rejected
        and all(row["detected_start_line"] is not None for row in rows)
        and max(errors, default=0) <= args.allowed_error_lines
        and (
            not args.analyze
            or (
                len(analysis_results) == len(sources)
                and not analysis_errors
            )
        )
    )
    report = {
        "schema_version": 1,
        "status": "pass" if passed else "fail",
        "strategy": "line-wise calibration plus hysteresis boundary state machine",
        "input": "30 uncorrected uint16 ENVI BIL RAW captures",
        "provenance": str(provenance_path),
        "sample_count": len(sources),
        "detected_sample_count": len(detections),
        "rejected_event_count": len(rejected),
        "gap_lines_between_files": args.gap_lines,
        "total_stream_lines": int(all_scores.size),
        "elapsed_seconds": elapsed,
        "lines_per_second": float(all_scores.size / max(elapsed, 1e-9)),
        "collect_cubes": bool(args.collect_cubes),
        "live_analysis_enabled": bool(args.analyze),
        "live_analysis_completed": len(analysis_results),
        "live_analysis_error_count": len(analysis_errors),
        "live_analysis_timed_out": bool(
            args.analyze
            and len(analysis_results) + len(analysis_errors) < analysis_submitted
        ),
        "allowed_error_lines": args.allowed_error_lines,
        "max_absolute_boundary_error_lines": max(errors, default=None),
        "detector": asdict(config),
        "calibrator": calibrator.metadata(),
        "samples": rows,
        "rejected_events": rejected,
    }
    return report, all_scores


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Simulate automatic sample boundaries from 30 uncorrected RAW captures."
    )
    parser.add_argument("--provenance", default=str(DEFAULT_PROVENANCE))
    parser.add_argument("--output-dir", default=str(DEFAULT_OUTPUT_DIR))
    parser.add_argument("--gap-lines", type=int, default=64)
    parser.add_argument("--chunk-lines", type=int, default=16)
    parser.add_argument("--trim-bands", type=int, default=10)
    parser.add_argument("--clip-max", type=float, default=1.2)
    parser.add_argument("--min-white-column-fraction", type=float, default=0.2)
    parser.add_argument("--material-pixel-threshold", type=float, default=0.08)
    parser.add_argument("--start-threshold", type=float, default=0.20)
    parser.add_argument("--end-threshold", type=float, default=0.04)
    parser.add_argument("--start-confirm-lines", type=int, default=4)
    parser.add_argument("--end-confirm-lines", type=int, default=8)
    parser.add_argument("--min-sample-lines", type=int, default=200)
    parser.add_argument("--max-sample-lines", type=int, default=1200)
    parser.add_argument("--allowed-error-lines", type=int, default=2)
    parser.add_argument(
        "--collect-cubes",
        action=argparse.BooleanOptionalAction,
        default=True,
        help="Accumulate each detected sample as a model-ready float32 cube.",
    )
    parser.add_argument(
        "--analyze",
        action=argparse.BooleanOptionalAction,
        default=False,
        help="Send every SAMPLE_END cube to the persistent CAE/resistance worker.",
    )
    parser.add_argument("--analysis-device", default="cuda")
    parser.add_argument("--analysis-timeout-seconds", type=float, default=300.0)
    return parser


def main() -> int:
    args = build_parser().parse_args()
    output_dir = Path(args.output_dir).resolve()
    output_dir.mkdir(parents=True, exist_ok=True)
    report, scores = run_simulation(args)
    report_path = output_dir / "stream_detection_report.json"
    csv_path = output_dir / "stream_detection_report.csv"
    plot_path = output_dir / "stream_detection_scores.png"
    report_path.write_text(
        json.dumps(report, ensure_ascii=False, indent=2),
        encoding="utf-8",
    )
    write_csv(csv_path, report["samples"])
    plotted = save_score_plot(plot_path, scores, report["samples"], DetectorConfig(**report["detector"]))
    print(f"[status] {report['status']}")
    print(f"[detected] {report['detected_sample_count']}/{report['sample_count']}")
    print(f"[max boundary error] {report['max_absolute_boundary_error_lines']} lines")
    print(f"[throughput] {report['lines_per_second']:.1f} lines/s")
    if report["live_analysis_enabled"]:
        print(
            "[live analysis] "
            f"{report['live_analysis_completed']}/{report['sample_count']} completed"
        )
    print(f"[report] {report_path}")
    print(f"[plot] {plot_path if plotted else 'matplotlib unavailable'}")
    return 0 if report["status"] == "pass" else 1


if __name__ == "__main__":
    raise SystemExit(main())
