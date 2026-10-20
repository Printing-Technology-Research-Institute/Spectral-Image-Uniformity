from __future__ import annotations

import csv
import json
import os
import queue
import statistics
import threading
import time
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


CSV_FIELDS = (
    "event_utc",
    "event_monotonic_s",
    "event",
    "frame_id",
    "camera_timestamp_ns",
    "camera_delta_ms",
    "frame_gap",
    "missing_frames",
    "fetch_wait_ms",
    "copy_ms",
    "processing_queue_depth",
    "processing_queue_drops",
    "block_lines",
    "block_processing_ms",
    "block_budget_ms",
    "block_over_budget",
    "consecutive_timeouts",
    "process_cpu_percent",
    "process_rss_mb",
    "available_ram_mb",
)


def _percentile(values: list[float], percentile: float) -> float | None:
    if not values:
        return None
    ordered = sorted(float(value) for value in values)
    position = (len(ordered) - 1) * float(percentile) / 100.0
    lower = int(position)
    upper = min(lower + 1, len(ordered) - 1)
    fraction = position - lower
    return ordered[lower] * (1.0 - fraction) + ordered[upper] * fraction


def _windows_memory_status() -> tuple[float | None, float | None]:
    """Return process RSS and system available RAM in MiB on Windows."""
    if os.name != "nt":
        return None, None
    try:
        import ctypes
        from ctypes import wintypes

        class PROCESS_MEMORY_COUNTERS(ctypes.Structure):
            _fields_ = [
                ("cb", wintypes.DWORD),
                ("PageFaultCount", wintypes.DWORD),
                ("PeakWorkingSetSize", ctypes.c_size_t),
                ("WorkingSetSize", ctypes.c_size_t),
                ("QuotaPeakPagedPoolUsage", ctypes.c_size_t),
                ("QuotaPagedPoolUsage", ctypes.c_size_t),
                ("QuotaPeakNonPagedPoolUsage", ctypes.c_size_t),
                ("QuotaNonPagedPoolUsage", ctypes.c_size_t),
                ("PagefileUsage", ctypes.c_size_t),
                ("PeakPagefileUsage", ctypes.c_size_t),
            ]

        class MEMORYSTATUSEX(ctypes.Structure):
            _fields_ = [
                ("dwLength", wintypes.DWORD),
                ("dwMemoryLoad", wintypes.DWORD),
                ("ullTotalPhys", ctypes.c_ulonglong),
                ("ullAvailPhys", ctypes.c_ulonglong),
                ("ullTotalPageFile", ctypes.c_ulonglong),
                ("ullAvailPageFile", ctypes.c_ulonglong),
                ("ullTotalVirtual", ctypes.c_ulonglong),
                ("ullAvailVirtual", ctypes.c_ulonglong),
                ("ullAvailExtendedVirtual", ctypes.c_ulonglong),
            ]

        counters = PROCESS_MEMORY_COUNTERS()
        counters.cb = ctypes.sizeof(counters)
        process = ctypes.windll.kernel32.GetCurrentProcess()
        ok_process = ctypes.windll.psapi.GetProcessMemoryInfo(
            process,
            ctypes.byref(counters),
            counters.cb,
        )
        memory = MEMORYSTATUSEX()
        memory.dwLength = ctypes.sizeof(memory)
        ok_system = ctypes.windll.kernel32.GlobalMemoryStatusEx(
            ctypes.byref(memory)
        )
        divisor = 1024.0 * 1024.0
        rss_mb = float(counters.WorkingSetSize) / divisor if ok_process else None
        available_mb = float(memory.ullAvailPhys) / divisor if ok_system else None
        return rss_mb, available_mb
    except Exception:
        return None, None


@dataclass(frozen=True)
class DiagnosticsPaths:
    csv_path: Path
    summary_path: Path


class AcquisitionDiagnosticsLogger:
    """Low-overhead acquisition logger with all file I/O off the fetch thread."""

    def __init__(
        self,
        log_dir: str | Path | None,
        *,
        enabled: bool = True,
        expected_line_rate_hz: float | None = None,
        queue_capacity: int = 16384,
        session_label: str | None = None,
        session_metadata: dict[str, Any] | None = None,
    ) -> None:
        self.enabled = bool(enabled and log_dir is not None)
        self.expected_line_rate_hz = (
            None
            if expected_line_rate_hz is None
            else float(expected_line_rate_hz)
        )
        self.paths: DiagnosticsPaths | None = None
        self.session_metadata = dict(session_metadata or {})
        self._events: queue.Queue[dict[str, Any] | None] = queue.Queue(
            maxsize=max(128, int(queue_capacity))
        )
        self._telemetry_drops = 0
        self._drop_lock = threading.Lock()
        self._closed = threading.Event()
        self._thread: threading.Thread | None = None
        if not self.enabled:
            return

        directory = Path(log_dir).expanduser().resolve()
        directory.mkdir(parents=True, exist_ok=True)
        stamp = datetime.now().strftime("%Y%m%d_%H%M%S_%f")
        label = "" if not session_label else f"_{session_label.strip()}"
        stem = f"fx17_acquisition_{stamp}{label}"
        self.paths = DiagnosticsPaths(
            csv_path=directory / f"{stem}.csv",
            summary_path=directory / f"{stem}_summary.json",
        )
        self._thread = threading.Thread(
            target=self._run,
            name="fx17-acquisition-logger",
            daemon=True,
        )
        self._thread.start()

    @property
    def telemetry_drops(self) -> int:
        with self._drop_lock:
            return int(self._telemetry_drops)

    def _submit(self, event: dict[str, Any]) -> None:
        if not self.enabled or self._closed.is_set():
            return
        try:
            self._events.put_nowait(event)
        except queue.Full:
            with self._drop_lock:
                self._telemetry_drops += 1

    def record_frame(
        self,
        *,
        frame_id: int | None,
        camera_timestamp_ns: int | None,
        fetch_wait_ms: float,
        copy_ms: float,
        processing_queue_depth: int,
        processing_queue_drops: int,
    ) -> None:
        self._submit(
            {
                "event": "frame",
                "event_utc": datetime.now(timezone.utc).isoformat(),
                "event_monotonic_s": time.monotonic(),
                "frame_id": frame_id,
                "camera_timestamp_ns": camera_timestamp_ns,
                "fetch_wait_ms": float(fetch_wait_ms),
                "copy_ms": float(copy_ms),
                "processing_queue_depth": int(processing_queue_depth),
                "processing_queue_drops": int(processing_queue_drops),
            }
        )

    def record_timeout(self, *, consecutive_timeouts: int) -> None:
        self._submit(
            {
                "event": "timeout",
                "event_utc": datetime.now(timezone.utc).isoformat(),
                "event_monotonic_s": time.monotonic(),
                "consecutive_timeouts": int(consecutive_timeouts),
            }
        )

    def record_block(
        self,
        *,
        block_lines: int,
        processing_ms: float,
        processing_queue_depth: int,
        processing_queue_drops: int,
    ) -> None:
        budget_ms = (
            None
            if not self.expected_line_rate_hz
            else float(block_lines) / self.expected_line_rate_hz * 1000.0
        )
        self._submit(
            {
                "event": "processing_block",
                "event_utc": datetime.now(timezone.utc).isoformat(),
                "event_monotonic_s": time.monotonic(),
                "processing_queue_depth": int(processing_queue_depth),
                "processing_queue_drops": int(processing_queue_drops),
                "block_lines": int(block_lines),
                "block_processing_ms": float(processing_ms),
                "block_budget_ms": budget_ms,
                "block_over_budget": (
                    None if budget_ms is None else processing_ms > budget_ms
                ),
            }
        )

    def close(self, timeout: float = 3.0) -> None:
        if not self.enabled or self._closed.is_set():
            return
        self._closed.set()
        deadline = time.monotonic() + max(0.1, float(timeout))
        while True:
            try:
                self._events.put(None, timeout=0.05)
                break
            except queue.Full:
                if time.monotonic() >= deadline:
                    return
        if self._thread is not None:
            self._thread.join(timeout=max(0.0, deadline - time.monotonic()))

    def _run(self) -> None:
        assert self.paths is not None
        started_utc = datetime.now(timezone.utc).isoformat()
        started_monotonic = time.monotonic()
        previous_frame_id: int | None = None
        previous_camera_timestamp: int | None = None
        frame_count = 0
        frame_id_resets = 0
        frame_gap_events = 0
        missing_frames = 0
        timeout_events = 0
        max_consecutive_timeouts = 0
        maximum_queue_depth = 0
        maximum_queue_drops = 0
        block_times: list[float] = []
        block_count = 0
        over_budget_blocks = 0
        max_rss_mb: float | None = None
        min_available_ram_mb: float | None = None
        cpu_samples: list[float] = []
        fetch_wait_times: list[float] = []
        copy_times: list[float] = []
        camera_delta_times: list[float] = []
        previous_cpu_time = time.process_time()
        previous_cpu_wall = time.monotonic()
        last_resource_sample = 0.0

        with self.paths.csv_path.open("w", encoding="utf-8-sig", newline="") as stream:
            writer = csv.DictWriter(stream, fieldnames=CSV_FIELDS)
            writer.writeheader()
            last_flush = time.monotonic()
            while True:
                event = self._events.get()
                if event is None:
                    break
                event_name = str(event.get("event", ""))
                now = float(event.get("event_monotonic_s", time.monotonic()))
                if now - last_resource_sample >= 1.0:
                    cpu_now = time.process_time()
                    wall_now = time.monotonic()
                    wall_delta = max(wall_now - previous_cpu_wall, 1e-9)
                    cpu_percent = (cpu_now - previous_cpu_time) / wall_delta * 100.0
                    previous_cpu_time = cpu_now
                    previous_cpu_wall = wall_now
                    rss_mb, available_mb = _windows_memory_status()
                    event["process_cpu_percent"] = cpu_percent
                    event["process_rss_mb"] = rss_mb
                    event["available_ram_mb"] = available_mb
                    cpu_samples.append(cpu_percent)
                    if rss_mb is not None:
                        max_rss_mb = (
                            rss_mb if max_rss_mb is None else max(max_rss_mb, rss_mb)
                        )
                    if available_mb is not None:
                        min_available_ram_mb = (
                            available_mb
                            if min_available_ram_mb is None
                            else min(min_available_ram_mb, available_mb)
                        )
                    last_resource_sample = now

                if event_name == "frame":
                    frame_count += 1
                    fetch_wait_times.append(
                        float(event.get("fetch_wait_ms") or 0.0)
                    )
                    copy_times.append(float(event.get("copy_ms") or 0.0))
                    frame_id = event.get("frame_id")
                    if frame_id is not None and previous_frame_id is not None:
                        difference = int(frame_id) - previous_frame_id
                        if difference > 1:
                            gap = difference - 1
                            event["frame_gap"] = gap
                            event["missing_frames"] = gap
                            frame_gap_events += 1
                            missing_frames += gap
                        elif difference <= 0:
                            frame_id_resets += 1
                    if frame_id is not None:
                        previous_frame_id = int(frame_id)
                    camera_timestamp = event.get("camera_timestamp_ns")
                    if (
                        camera_timestamp is not None
                        and previous_camera_timestamp is not None
                    ):
                        event["camera_delta_ms"] = (
                            int(camera_timestamp) - previous_camera_timestamp
                        ) / 1_000_000.0
                        camera_delta_times.append(
                            float(event["camera_delta_ms"])
                        )
                    if camera_timestamp is not None:
                        previous_camera_timestamp = int(camera_timestamp)
                elif event_name == "timeout":
                    timeout_events += 1
                    max_consecutive_timeouts = max(
                        max_consecutive_timeouts,
                        int(event.get("consecutive_timeouts", 0)),
                    )
                elif event_name == "processing_block":
                    block_count += 1
                    duration = float(event.get("block_processing_ms", 0.0))
                    block_times.append(duration)
                    if event.get("block_over_budget") is True:
                        over_budget_blocks += 1

                maximum_queue_depth = max(
                    maximum_queue_depth,
                    int(event.get("processing_queue_depth") or 0),
                )
                maximum_queue_drops = max(
                    maximum_queue_drops,
                    int(event.get("processing_queue_drops") or 0),
                )
                writer.writerow(
                    {field: event.get(field) for field in CSV_FIELDS}
                )
                if now - last_flush >= 1.0:
                    stream.flush()
                    last_flush = now

        ended_monotonic = time.monotonic()
        summary = {
            "schema_version": 1,
            "started_utc": started_utc,
            "ended_utc": datetime.now(timezone.utc).isoformat(),
            "duration_seconds": ended_monotonic - started_monotonic,
            "expected_line_rate_hz": self.expected_line_rate_hz,
            "configuration": self.session_metadata,
            "frames_fetched": frame_count,
            "frame_id_gap_events": frame_gap_events,
            "missing_camera_frames": missing_frames,
            "frame_id_resets": frame_id_resets,
            "timeout_events": timeout_events,
            "max_consecutive_timeouts": max_consecutive_timeouts,
            "processing_queue_max_depth": maximum_queue_depth,
            "processing_queue_drops": maximum_queue_drops,
            "diagnostics_queue_drops": self.telemetry_drops,
            "processing_blocks": block_count,
            "processing_blocks_over_budget": over_budget_blocks,
            "processing_ms_mean": (
                statistics.fmean(block_times) if block_times else None
            ),
            "processing_ms_p50": _percentile(block_times, 50.0),
            "processing_ms_p95": _percentile(block_times, 95.0),
            "processing_ms_max": max(block_times) if block_times else None,
            "fetch_wait_ms_mean": (
                statistics.fmean(fetch_wait_times)
                if fetch_wait_times
                else None
            ),
            "fetch_wait_ms_p95": _percentile(fetch_wait_times, 95.0),
            "fetch_wait_ms_max": (
                max(fetch_wait_times) if fetch_wait_times else None
            ),
            "copy_ms_mean": (
                statistics.fmean(copy_times) if copy_times else None
            ),
            "copy_ms_p95": _percentile(copy_times, 95.0),
            "copy_ms_max": max(copy_times) if copy_times else None,
            "camera_delta_ms_mean": (
                statistics.fmean(camera_delta_times)
                if camera_delta_times
                else None
            ),
            "camera_delta_ms_p95": _percentile(camera_delta_times, 95.0),
            "camera_delta_ms_max": (
                max(camera_delta_times) if camera_delta_times else None
            ),
            "process_cpu_percent_mean": (
                statistics.fmean(cpu_samples) if cpu_samples else None
            ),
            "process_cpu_percent_max": max(cpu_samples) if cpu_samples else None,
            "process_rss_mb_max": max_rss_mb,
            "available_ram_mb_min": min_available_ram_mb,
            "csv_path": str(self.paths.csv_path),
        }
        self.paths.summary_path.write_text(
            json.dumps(summary, ensure_ascii=False, indent=2),
            encoding="utf-8",
        )
