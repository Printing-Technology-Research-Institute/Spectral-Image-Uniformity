from __future__ import annotations

import argparse
import json
import platform
import sys
import time
import traceback
from pathlib import Path
from typing import Any

import numpy as np
from genicam import genapi
from genicam.gentl import TimeoutException
from harvesters.core import Harvester


APP_DIR = Path(__file__).resolve().parent
DEFAULT_CTI_PATH = APP_DIR / "mvGenTLProducer.cti"
EXPECTED_SHAPE = (224, 640)


def _create_acquirer(harvester: Harvester, device_index: int) -> Any:
    create = getattr(harvester, "create", None)
    if create is not None:
        return create(device_index)
    return harvester.create_image_acquirer(list_index=device_index)


def _start_acquisition(image_acquirer: Any) -> None:
    start = getattr(image_acquirer, "start", None)
    if start is not None:
        start()
    else:
        image_acquirer.start_acquisition()


def _stop_acquisition(image_acquirer: Any) -> None:
    stop = getattr(image_acquirer, "stop", None)
    if stop is not None:
        stop()
    else:
        image_acquirer.stop_acquisition()


def _fetch_buffer(image_acquirer: Any, timeout: float) -> Any:
    fetch = getattr(image_acquirer, "fetch", None)
    if fetch is not None:
        return fetch(timeout=timeout)
    return image_acquirer.fetch_buffer(timeout=timeout)


def _plain(value: Any) -> Any:
    if value is None or isinstance(value, (bool, int, float, str)):
        return value
    if isinstance(value, np.generic):
        return value.item()
    if isinstance(value, (list, tuple)):
        return [_plain(item) for item in value]
    if isinstance(value, dict):
        return {str(key): _plain(item) for key, item in value.items()}
    return str(value)


def _safe_attribute(value: Any, name: str) -> Any:
    try:
        return _plain(getattr(value, name))
    except Exception as exc:
        return f"<unavailable: {type(exc).__name__}: {exc}>"


def _safe_call(value: Any, name: str) -> Any:
    try:
        return _plain(getattr(value, name)())
    except Exception as exc:
        return f"<unavailable: {type(exc).__name__}: {exc}>"


def _device_record(device: Any, index: int) -> dict[str, Any]:
    properties = {
        str(key): _plain(value)
        for key, value in getattr(device, "property_dict", {}).items()
    }
    return {
        "index": index,
        "display": str(device),
        "properties": properties,
    }


def _node_record(node: Any) -> dict[str, Any]:
    record: dict[str, Any] = {
        "name": _safe_attribute(node, "name"),
        "display_name": _safe_attribute(node, "display_name"),
        "interface_type": _safe_attribute(node, "principal_interface_type"),
        "access_mode": _safe_call(node, "get_access_mode"),
    }
    try:
        readable = bool(genapi.is_readable(node))
    except Exception:
        readable = False
    record["readable"] = readable

    value_interfaces = (
        genapi.IInteger,
        genapi.IFloat,
        genapi.IEnumeration,
        genapi.IString,
        genapi.IBoolean,
    )
    if readable and isinstance(node, value_interfaces):
        record["value"] = _safe_attribute(node, "value")
    if readable and isinstance(node, (genapi.IInteger, genapi.IFloat)):
        for name in ("min", "max", "inc", "unit"):
            record[name] = _safe_attribute(node, name)
    if readable and isinstance(node, genapi.IEnumeration):
        record["symbolics"] = _safe_attribute(node, "symbolics")
    return record


def _read_node_map(node_map: Any) -> list[dict[str, Any]]:
    records: list[dict[str, Any]] = []
    for node in node_map.nodes:
        try:
            records.append(_node_record(node))
        except Exception as exc:
            records.append(
                {
                    "name": _safe_attribute(node, "name"),
                    "diagnostic_error": f"{type(exc).__name__}: {exc}",
                }
            )
    records.sort(key=lambda item: str(item.get("name", "")))
    return records


def _component_record(component: Any, copied: np.ndarray) -> dict[str, Any]:
    width = int(component.width)
    height = int(component.height)
    components_per_pixel = int(component.num_components_per_pixel)
    expected_elements = width * height * components_per_pixel
    reshaped = None
    if components_per_pixel == 1 and copied.size == width * height:
        reshaped = copied.reshape(height, width)
    return {
        "width": width,
        "height": height,
        "shape": [height, width],
        "data_size": int(copied.size),
        "expected_elements": expected_elements,
        "dtype": str(copied.dtype),
        "itemsize": int(copied.dtype.itemsize),
        "nbytes": int(copied.nbytes),
        "data_format": _safe_attribute(component, "data_format"),
        "data_format_value": _safe_attribute(component, "data_format_value"),
        "components_per_pixel": components_per_pixel,
        "x_padding": _safe_attribute(component, "x_padding"),
        "y_padding": _safe_attribute(component, "y_padding"),
        "min": _plain(copied.min()) if copied.size else None,
        "max": _plain(copied.max()) if copied.size else None,
        "reshape_ok": reshaped is not None,
        "expected_fx17_shape": list(EXPECTED_SHAPE),
        "expected_fx17_shape_ok": (
            reshaped is not None and reshaped.shape == EXPECTED_SHAPE
        ),
        "copy_owns_memory": bool(copied.flags.owndata),
    }


def _frame_id(buffer: Any) -> int | None:
    try:
        return int(buffer.module.frame_id)
    except Exception:
        return None


def _timestamp_ns(buffer: Any) -> int | None:
    try:
        return int(buffer.timestamp_ns)
    except Exception:
        return None


def _acquire(
    image_acquirer: Any,
    *,
    buffer_count: int,
    timeout_seconds: float,
) -> tuple[dict[str, Any], np.ndarray | None]:
    first_component: dict[str, Any] | None = None
    first_copy: np.ndarray | None = None
    frame_ids: list[int] = []
    timestamps_ns: list[int] = []
    timeout_count = 0
    fetched_count = 0
    started = time.perf_counter()
    _start_acquisition(image_acquirer)
    try:
        attempts = 0
        max_attempts = buffer_count + max(10, buffer_count // 10)
        while fetched_count < buffer_count and attempts < max_attempts:
            attempts += 1
            try:
                buffer = _fetch_buffer(image_acquirer, timeout_seconds)
            except TimeoutException:
                timeout_count += 1
                continue
            if buffer is None:
                timeout_count += 1
                continue
            with buffer:
                components = buffer.payload.components
                if not components:
                    raise RuntimeError("The fetched buffer has no payload components.")
                component = components[0]
                # This copy must happen before the context manager re-queues the buffer.
                copied = np.array(component.data, copy=True)
                if first_component is None:
                    first_component = _component_record(component, copied)
                    first_copy = copied
                frame_id = _frame_id(buffer)
                if frame_id is not None:
                    frame_ids.append(frame_id)
                timestamp = _timestamp_ns(buffer)
                if timestamp is not None:
                    timestamps_ns.append(timestamp)
            fetched_count += 1
    finally:
        _stop_acquisition(image_acquirer)
    elapsed = time.perf_counter() - started

    frame_id_gaps = [
        current - previous
        for previous, current in zip(frame_ids, frame_ids[1:])
        if current > previous
    ]
    timestamp_deltas = np.diff(np.asarray(timestamps_ns, dtype=np.int64))
    positive_timestamp_deltas = timestamp_deltas[timestamp_deltas > 0]
    device_rate = None
    if positive_timestamp_deltas.size:
        device_rate = 1e9 / float(np.median(positive_timestamp_deltas))

    acquisition = {
        "requested_buffers": buffer_count,
        "fetched_buffers": fetched_count,
        "timeout_count": timeout_count,
        "elapsed_seconds": elapsed,
        "host_buffer_rate_hz": fetched_count / elapsed if elapsed > 0 else None,
        "device_timestamp_rate_hz": device_rate,
        "frame_id_first": frame_ids[0] if frame_ids else None,
        "frame_id_last": frame_ids[-1] if frame_ids else None,
        "frame_id_missing_count": (
            sum(max(0, gap - 1) for gap in frame_id_gaps)
            if frame_id_gaps
            else 0
        ),
        "component": first_component,
    }
    return acquisition, first_copy


def run_diagnostic(args: argparse.Namespace) -> tuple[int, dict[str, Any]]:
    cti_path = Path(args.cti).expanduser().resolve()
    report: dict[str, Any] = {
        "schema_version": 1,
        "read_only_camera_parameters": True,
        "safety": {
            "writes_genicam_features": False,
            "controls_shutter": False,
            "changes_exposure": False,
            "changes_pixel_format": False,
            "starts_and_stops_acquisition": True,
        },
        "runtime": {
            "python": sys.version,
            "executable": sys.executable,
            "platform": platform.platform(),
            "numpy": np.__version__,
        },
        "cti_path": str(cti_path),
        "devices": [],
    }
    if not cti_path.is_file():
        report["error"] = f"CTI file does not exist: {cti_path}"
        return 2, report

    harvester = Harvester()
    image_acquirer = None
    try:
        harvester.add_file(
            str(cti_path),
            check_existence=True,
            check_validity=True,
        )
        harvester.update()
        report["devices"] = [
            _device_record(device, index)
            for index, device in enumerate(harvester.device_info_list)
        ]
        if not report["devices"]:
            report["error"] = (
                "No GenTL device was discovered. Close other camera applications "
                "and verify the FX17/GigE Vision connection and NIC driver."
            )
            return 3, report
        if args.device_index < 0 or args.device_index >= len(report["devices"]):
            report["error"] = f"Device index {args.device_index} is out of range."
            return 4, report

        # Deliberately do not use camera.Camera._connect(): it writes default settings.
        image_acquirer = _create_acquirer(harvester, args.device_index)
        node_map = image_acquirer.remote_device.node_map
        nodes = _read_node_map(node_map)
        report["selected_device_index"] = args.device_index
        report["node_count"] = len(nodes)
        report["node_map"] = nodes
        acquisition, first_copy = _acquire(
            image_acquirer,
            buffer_count=args.buffers,
            timeout_seconds=args.timeout,
        )
        report["acquisition"] = acquisition
        if args.raw_output and first_copy is not None:
            raw_output = Path(args.raw_output).expanduser().resolve()
            first_copy.tofile(raw_output)
            report["raw_output"] = str(raw_output)
        component = acquisition.get("component")
        if not component or not component.get("reshape_ok"):
            report["error"] = "The first component could not be reshaped safely."
            return 5, report
        return 0, report
    except Exception:
        report["error"] = traceback.format_exc()
        return 1, report
    finally:
        if image_acquirer is not None:
            try:
                image_acquirer.destroy()
            except Exception:
                pass
        harvester.reset()


def _summary(report: dict[str, Any]) -> dict[str, Any]:
    acquisition = report.get("acquisition", {})
    return {
        "read_only_camera_parameters": report["read_only_camera_parameters"],
        "cti_path": report["cti_path"],
        "device_count": len(report.get("devices", [])),
        "selected_device_index": report.get("selected_device_index"),
        "node_count": report.get("node_count"),
        "acquisition": acquisition,
        "error": report.get("error"),
    }


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=(
            "Read-only FX17/GenTL diagnostic. It never writes camera features, "
            "changes exposure/pixel format, or controls the shutter."
        )
    )
    parser.add_argument("--cti", default=str(DEFAULT_CTI_PATH))
    parser.add_argument("--device-index", type=int, default=0)
    parser.add_argument("--buffers", type=int, default=1)
    parser.add_argument("--timeout", type=float, default=0.5)
    parser.add_argument(
        "--report",
        help="Optional JSON output containing the complete node map.",
    )
    parser.add_argument(
        "--raw-output",
        help="Optional raw copy of the first decoded component.",
    )
    return parser


def main() -> int:
    args = build_parser().parse_args()
    if args.buffers < 1:
        raise SystemExit("--buffers must be positive")
    if args.timeout <= 0:
        raise SystemExit("--timeout must be positive")
    code, report = run_diagnostic(args)
    if args.report:
        report_path = Path(args.report).expanduser().resolve()
        report_path.write_text(
            json.dumps(report, indent=2, ensure_ascii=False),
            encoding="utf-8",
        )
    print(json.dumps(_summary(report), indent=2, ensure_ascii=False))
    return code


if __name__ == "__main__":
    raise SystemExit(main())
