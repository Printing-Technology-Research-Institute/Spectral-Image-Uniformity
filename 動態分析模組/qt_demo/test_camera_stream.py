from __future__ import annotations

import json
import os
import queue
import tempfile
import threading
import time
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

import numpy as np

from camera_stream import (
    FX17CameraStreamWorker,
    _CalibrationRequest,
    _LineProcessingWorker,
    _configure_buffer_pool,
    _configure_mvimpact_runtime,
    _create_acquirer,
    _fetch_buffer,
    _start_acquisition,
    _stop_acquisition,
)
from acquisition_diagnostics import AcquisitionDiagnosticsLogger
from stream_source import StreamMessage


class _ModernHarvester:
    def __init__(self) -> None:
        self.created = None

    def create(self, index: int) -> str:
        self.created = index
        return "modern"


class _LegacyHarvester:
    def __init__(self) -> None:
        self.created = None

    def create_image_acquirer(self, *, list_index: int) -> str:
        self.created = list_index
        return "legacy"


class _ModernAcquirer:
    def __init__(self) -> None:
        self.calls: list[object] = []

    def start(self) -> None:
        self.calls.append("start")

    def stop(self) -> None:
        self.calls.append("stop")

    def fetch(self, *, timeout: float) -> str:
        self.calls.append(("fetch", timeout))
        return "modern-buffer"


class _LegacyAcquirer:
    def __init__(self) -> None:
        self.calls: list[object] = []

    def start_acquisition(self) -> None:
        self.calls.append("start")

    def stop_acquisition(self) -> None:
        self.calls.append("stop")

    def fetch_buffer(self, *, timeout: float) -> str:
        self.calls.append(("fetch", timeout))
        return "legacy-buffer"


class _BufferedAcquirer:
    def __init__(self, num_buffers: int) -> None:
        self.num_buffers = num_buffers


class _SlowPipeline:
    def __init__(self, delay_seconds: float = 0.0) -> None:
        self.delay_seconds = float(delay_seconds)
        self.blocks: list[tuple[list[int | None], list[int | None]]] = []
        self.reset_count = 0
        self.flush_count = 0

    def reset(self) -> None:
        self.reset_count += 1

    def process_block(
        self,
        raw_block: np.ndarray,
        *,
        sample_id: str,
        frame_ids: list[int | None],
        timestamps_ns: list[int | None],
    ) -> list[StreamMessage]:
        time.sleep(self.delay_seconds)
        self.blocks.append((list(frame_ids), list(timestamps_ns)))
        return [StreamMessage(kind="preview", sample_id=sample_id)]

    def flush(self, *, sample_id: str) -> list[StreamMessage]:
        self.flush_count += 1
        return []


class _RecordingNode:
    def __init__(self) -> None:
        self.writes: list[object] = []

    @property
    def value(self) -> object | None:
        return self.writes[-1] if self.writes else None

    @value.setter
    def value(self, value: object) -> None:
        self.writes.append(value)


class _ValueNode:
    def __init__(self, value: float | bool) -> None:
        self._value = value
        self.writes: list[float | bool] = []

    @property
    def value(self) -> float | bool:
        return self._value

    @value.setter
    def value(self, value: float | bool) -> None:
        self._value = value
        self.writes.append(value)


class _ClampedNode(_ValueNode):
    def __init__(self, value: float, clamped_value: float) -> None:
        super().__init__(value)
        self.clamped_value = clamped_value

    @_ValueNode.value.setter
    def value(self, value: float | bool) -> None:
        self._value = self.clamped_value
        self.writes.append(value)


class _RetryingHarvester:
    def __init__(self, devices_by_attempt: list[list[object]]) -> None:
        self.devices_by_attempt = devices_by_attempt
        self.update_count = 0
        self.device_info_list: list[object] = []

    def update(self) -> None:
        index = min(self.update_count, len(self.devices_by_attempt) - 1)
        self.device_info_list = self.devices_by_attempt[index]
        self.update_count += 1


class CameraStreamCompatibilityTests(unittest.TestCase):
    def test_configures_portable_mvimpact_runtime(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / "runtime"
            cti_path = root / "bin" / "x64" / "mvGenTLProducer.cti"
            cti_path.parent.mkdir(parents=True)
            cti_path.touch()
            data_dir = Path(directory) / "data"

            with patch.dict(os.environ, {"PATH": "existing"}, clear=False):
                configured = _configure_mvimpact_runtime(
                    cti_path,
                    data_dir=data_dir,
                )

                self.assertEqual(configured, root.resolve())
                self.assertEqual(
                    os.environ["MVIMPACT_ACQUIRE_DIR"],
                    str(root.resolve()),
                )
                self.assertEqual(
                    os.environ["MVIMPACT_ACQUIRE_DATA_DIR"],
                    str(data_dir.resolve()),
                )
                self.assertEqual(
                    os.environ["PATH"].split(os.pathsep)[:2],
                    [str(cti_path.parent.resolve()), str(root.resolve())],
                )
                self.assertTrue(data_dir.is_dir())

    @staticmethod
    def line_rate_features(
        *,
        enabled: bool = False,
        rate: float = 200.0,
        minimum: float = 2.0,
        maximum: float = 330.0,
    ) -> dict:
        return {
            "AcquisitionControl": {
                "EnAcquisitionFrameRate": _ValueNode(enabled),
                "AcquisitionFrameRate": _ValueNode(rate),
                "AcquisitionFrameRateMin": _ValueNode(minimum),
                "AcquisitionFrameRateMax": _ValueNode(maximum),
            }
        }

    def test_supports_harvester_13_and_14_method_names(self) -> None:
        modern_harvester = _ModernHarvester()
        legacy_harvester = _LegacyHarvester()
        self.assertEqual(_create_acquirer(modern_harvester, 2), "modern")
        self.assertEqual(_create_acquirer(legacy_harvester, 3), "legacy")
        self.assertEqual(modern_harvester.created, 2)
        self.assertEqual(legacy_harvester.created, 3)

        for acquirer, expected in (
            (_ModernAcquirer(), "modern-buffer"),
            (_LegacyAcquirer(), "legacy-buffer"),
        ):
            _start_acquisition(acquirer)
            self.assertEqual(_fetch_buffer(acquirer, 0.25), expected)
            _stop_acquisition(acquirer)
            self.assertEqual(
                acquirer.calls,
                ["start", ("fetch", 0.25), "stop"],
            )

    def test_increases_harvester_buffer_pool_without_shrinking_it(self) -> None:
        small = _BufferedAcquirer(8)
        large = _BufferedAcquirer(256)

        self.assertEqual(_configure_buffer_pool(small, 128), 128)
        self.assertEqual(_configure_buffer_pool(large, 128), 256)
        self.assertEqual(small.num_buffers, 128)
        self.assertEqual(large.num_buffers, 256)

    def test_applies_sample_camera_settings_and_shutter_pulses(self) -> None:
        frame_rate = _RecordingNode()
        pixel_format = _RecordingNode()
        exposure = _RecordingNode()
        shutter_rev = _RecordingNode()
        shutter_fwd = _RecordingNode()
        features = {
            "AcquisitionControl": {
                "EnAcquisitionFrameRate": frame_rate,
                "ExposureTime": exposure,
            },
            "ImageFormatControl": {"PixelFormat": pixel_format},
            "Specim": {
                "MotorShutter": {
                    "MotorShutter_PulseRev": shutter_rev,
                    "MotorShutter_PulseFwd": shutter_fwd,
                }
            },
        }
        worker = object.__new__(FX17CameraStreamWorker)
        worker.exposure_us = 5000.0

        worker._configure_features(features)
        worker._set_shutter(features, opened=True)
        worker._set_shutter(features, opened=False)

        self.assertEqual(frame_rate.writes, [False])
        self.assertEqual(pixel_format.writes, ["Mono12Packed"])
        self.assertEqual(exposure.writes, [5000.0])
        self.assertEqual(shutter_rev.writes, [190, 200])
        self.assertEqual(shutter_fwd.writes, [190, 200])

    def test_retries_transient_device_discovery(self) -> None:
        device = object()
        harvester = _RetryingHarvester([[], [], [device]])
        worker = object.__new__(FX17CameraStreamWorker)
        worker.discovery_attempts = 4
        worker.discovery_retry_interval = 0.0
        worker._stop_event = threading.Event()

        devices = worker._discover_devices(harvester)

        self.assertEqual(devices, [device])
        self.assertEqual(harvester.update_count, 3)

    @staticmethod
    def device(model: str, serial: str, vendor: str = "Specim") -> object:
        return SimpleNamespace(
            model=model,
            serial_number=serial,
            vendor=vendor,
            id_=f"{model}({serial})",
        )

    def test_prefers_the_configured_fx17_serial(self) -> None:
        worker = object.__new__(FX17CameraStreamWorker)
        worker.configured_serial_number = "SERIAL-A"
        devices = [
            self.device("FX17e", "SERIAL-B"),
            self.device("FX17e", "SERIAL-A"),
        ]

        self.assertEqual(worker._select_device_index(devices), 1)

    def test_auto_selects_the_only_discovered_fx17(self) -> None:
        worker = object.__new__(FX17CameraStreamWorker)
        worker.configured_serial_number = "OLD-SERIAL"
        devices = [
            self.device("OtherCamera", "OTHER", "OtherVendor"),
            self.device("FX17e", "NEW-SERIAL"),
        ]

        self.assertEqual(worker._select_device_index(devices), 1)

    def test_refuses_to_guess_when_multiple_fx17_cameras_are_present(self) -> None:
        worker = object.__new__(FX17CameraStreamWorker)
        worker.configured_serial_number = "MISSING-SERIAL"
        devices = [
            self.device("FX17e", "SERIAL-A"),
            self.device("FX17", "SERIAL-B"),
        ]

        with self.assertRaisesRegex(RuntimeError, "Multiple FX17"):
            worker._select_device_index(devices)

    def test_free_running_line_rate_uses_reported_maximum(self) -> None:
        status = FX17CameraStreamWorker._read_line_rate(
            self.line_rate_features(enabled=False, maximum=331.92)
        )

        self.assertFalse(status["line_rate_control_enabled"])
        self.assertAlmostEqual(status["line_rate_hz"], 331.92)

    def test_writes_enables_and_reads_back_line_rate(self) -> None:
        features = self.line_rate_features(enabled=False, maximum=331.92)

        status = FX17CameraStreamWorker._write_line_rate(features, 141.75)

        acquisition = features["AcquisitionControl"]
        self.assertEqual(
            acquisition["EnAcquisitionFrameRate"].writes,
            [True],
        )
        self.assertEqual(
            acquisition["AcquisitionFrameRate"].writes,
            [141.75],
        )
        self.assertAlmostEqual(status["line_rate_hz"], 141.75)
        self.assertTrue(status["line_rate_control_enabled"])

    def test_rejects_line_rate_outside_camera_range(self) -> None:
        with self.assertRaisesRegex(ValueError, "outside the FX17 range"):
            FX17CameraStreamWorker._write_line_rate(
                self.line_rate_features(minimum=2.0, maximum=330.0),
                500.0,
            )

    def test_readback_mismatch_restores_free_running_mode(self) -> None:
        features = self.line_rate_features(enabled=False, maximum=331.92)
        acquisition = features["AcquisitionControl"]
        acquisition["AcquisitionFrameRate"] = _ClampedNode(200.0, 120.0)

        with self.assertRaisesRegex(RuntimeError, "read-back mismatch"):
            FX17CameraStreamWorker._write_line_rate(features, 141.75)

        self.assertEqual(
            acquisition["EnAcquisitionFrameRate"].writes,
            [True, False],
        )


class CameraBufferTests(unittest.TestCase):
    def make_worker_shell(self) -> FX17CameraStreamWorker:
        worker = object.__new__(FX17CameraStreamWorker)
        worker.bands = 4
        worker.width = 5
        return worker

    def make_buffer(
        self,
        data: np.ndarray,
        *,
        height: int = 4,
        width: int = 5,
        components_per_pixel: int = 1,
        data_format: str = "Mono12Packed",
    ) -> SimpleNamespace:
        component = SimpleNamespace(
            data=data,
            height=height,
            width=width,
            num_components_per_pixel=components_per_pixel,
            data_format=data_format,
        )
        return SimpleNamespace(
            payload=SimpleNamespace(components=[component])
        )

    def test_copies_decoded_component_before_buffer_release(self) -> None:
        worker = self.make_worker_shell()
        source = np.arange(20, dtype=np.uint16)
        copied, data_format = worker._copy_component(self.make_buffer(source))
        source.fill(0)

        self.assertEqual(copied.shape, (4, 5))
        self.assertEqual(copied.dtype, np.uint16)
        self.assertEqual(data_format, "Mono12Packed")
        self.assertEqual(int(copied[-1, -1]), 19)
        self.assertTrue(copied.flags.c_contiguous)

    def test_rejects_packed_bytes_that_harvester_did_not_decode(self) -> None:
        worker = self.make_worker_shell()
        packed = np.zeros(30, dtype=np.uint8)

        with self.assertRaisesRegex(ValueError, "Packed data was not decoded"):
            worker._copy_component(self.make_buffer(packed))

    def test_rejects_unexpected_component_geometry(self) -> None:
        worker = self.make_worker_shell()
        data = np.zeros(20, dtype=np.uint16)

        with self.assertRaisesRegex(ValueError, "Expected FX17 component"):
            worker._copy_component(
                self.make_buffer(data, height=5, width=4)
            )


class CameraCalibrationCaptureTests(unittest.TestCase):
    def test_selected_calibration_pair_reloads_pipeline_atomically(self) -> None:
        worker = object.__new__(FX17CameraStreamWorker)
        worker._commands = queue.Queue()
        worker._messages = queue.Queue()
        worker._active_calibration = None
        worker._pending_pipeline_replacement = None
        worker.dark_ref_path = Path("old-dark.dat")
        worker.white_ref_path = Path("old-white.dat")
        replacement_pipeline = object()
        worker._build_pipeline = lambda: replacement_pipeline

        with tempfile.TemporaryDirectory() as directory:
            dark_path = Path(directory) / "new-dark.dat"
            white_path = Path(directory) / "new-white.dat"
            worker.set_calibration_files(dark_path, white_path)
            shutter_open = worker._apply_commands(
                {},
                shutter_is_open=True,
            )

        original_pipeline = object()
        active_pipeline = worker._take_pipeline_replacement(original_pipeline)
        messages = worker.poll()

        self.assertTrue(shutter_open)
        self.assertIs(active_pipeline, replacement_pipeline)
        self.assertEqual(worker.dark_ref_path, dark_path)
        self.assertEqual(worker.white_ref_path, white_path)
        self.assertEqual(messages[-1].kind, "calibration_files_loaded")
        self.assertEqual(messages[-1].dark_ref_path, str(dark_path))
        self.assertEqual(messages[-1].white_ref_path, str(white_path))

    def test_invalid_selected_pair_keeps_active_calibration(self) -> None:
        worker = object.__new__(FX17CameraStreamWorker)
        worker._commands = queue.Queue()
        worker._messages = queue.Queue()
        worker._active_calibration = None
        worker._pending_pipeline_replacement = None
        old_dark = Path("old-dark.dat")
        old_white = Path("old-white.dat")
        worker.dark_ref_path = old_dark
        worker.white_ref_path = old_white

        def reject_pair() -> None:
            raise ValueError("White reference must be greater than dark reference")

        worker._build_pipeline = reject_pair
        worker.set_calibration_files("bad-dark.dat", "bad-white.dat")
        worker._apply_commands({}, shutter_is_open=False)
        messages = worker.poll()

        self.assertEqual(worker.dark_ref_path, old_dark)
        self.assertEqual(worker.white_ref_path, old_white)
        self.assertIsNone(worker._pending_pipeline_replacement)
        self.assertEqual(messages[-1].kind, "calibration_files_failed")
        self.assertIn("White reference", messages[-1].error)

    def test_queues_capture_request_with_configured_defaults(self) -> None:
        worker = object.__new__(FX17CameraStreamWorker)
        worker.calibration_frame_count = 32
        worker.calibration_settle_frames = 7
        worker.calibration_settle_seconds = 0.4
        worker._commands = queue.Queue()

        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "dark.dat"
            worker.capture_reference("dark", output)
            command, request = worker._commands.get_nowait()

        self.assertEqual(command, "capture_reference")
        self.assertIsInstance(request, _CalibrationRequest)
        self.assertEqual(request.kind, "dark")
        self.assertEqual(request.path, output)
        self.assertEqual(request.frame_count, 32)
        self.assertEqual(request.settle_frames, 7)
        self.assertEqual(request.settle_seconds, 0.4)
        self.assertTrue(request.activate_after_capture)

    def test_writes_float32_reference_and_metadata(self) -> None:
        worker = object.__new__(FX17CameraStreamWorker)
        worker.bands = 4
        worker.width = 5
        worker.exposure_us = 1200.0
        worker.serial_number = "076900034157"

        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "white.dat"
            request = _CalibrationRequest(
                kind="white",
                path=output,
                frame_count=64,
                settle_frames=8,
                settle_seconds=0.5,
            )
            reference = np.arange(20, dtype=np.float32).reshape(4, 5)
            worker._write_reference_files(request, reference)

            stored = np.fromfile(output, dtype="<f4").reshape(4, 5)
            metadata = json.loads(
                output.with_suffix(".dat.json").read_text(encoding="utf-8")
            )

        np.testing.assert_array_equal(stored, reference)
        self.assertEqual(metadata["calibration_kind"], "white")
        self.assertEqual(metadata["shape"], [4, 5])
        self.assertEqual(metadata["frame_count"], 64)
        self.assertEqual(metadata["exposure_us"], 1200.0)
        self.assertEqual(metadata["camera_serial_number"], "076900034157")

    def test_dark_capture_averages_frames_reloads_and_restores_shutter(
        self,
    ) -> None:
        shutter_rev = _RecordingNode()
        shutter_fwd = _RecordingNode()
        features = {
            "Specim": {
                "MotorShutter": {
                    "MotorShutter_PulseRev": shutter_rev,
                    "MotorShutter_PulseFwd": shutter_fwd,
                }
            }
        }
        worker = object.__new__(FX17CameraStreamWorker)
        worker._commands = queue.Queue()
        worker._messages = queue.Queue()
        worker._active_calibration = None
        worker.dark_ref_path = Path("old-dark.dat")
        worker.white_ref_path = Path("white.dat")
        captured: dict[str, np.ndarray] = {}
        replacement_pipeline = object()
        worker._write_reference_files = (
            lambda request, reference: captured.setdefault(
                "reference",
                reference.copy(),
            )
        )
        worker._build_pipeline = lambda: replacement_pipeline
        request = _CalibrationRequest(
            kind="dark",
            path=Path("new-dark.dat"),
            frame_count=2,
            settle_frames=0,
            settle_seconds=0.0,
        )
        worker._commands.put(("capture_reference", request))

        shutter_open = worker._apply_commands(
            features,
            shutter_is_open=True,
        )
        original_pipeline = object()
        pipeline, shutter_open, consumed = worker._consume_calibration_line(
            np.full((2, 3), 2, dtype=np.uint16),
            original_pipeline,
            features,
            shutter_is_open=shutter_open,
        )
        self.assertIs(pipeline, original_pipeline)
        self.assertTrue(consumed)
        pipeline, shutter_open, consumed = worker._consume_calibration_line(
            np.full((2, 3), 4, dtype=np.uint16),
            pipeline,
            features,
            shutter_is_open=shutter_open,
        )

        self.assertIs(pipeline, replacement_pipeline)
        self.assertTrue(shutter_open)
        self.assertTrue(consumed)
        self.assertEqual(worker.dark_ref_path, Path("new-dark.dat"))
        np.testing.assert_array_equal(
            captured["reference"],
            np.full((2, 3), 3, dtype=np.float32),
        )
        self.assertEqual(shutter_fwd.writes, [190, 200])
        self.assertEqual(shutter_rev.writes, [190, 200])
        messages = worker.poll()
        self.assertEqual(
            [message.kind for message in messages],
            ["calibration_started", "calibration_complete"],
        )
        self.assertTrue(messages[-1].calibration_activated)

    def test_pair_dark_stage_does_not_reload_pipeline(self) -> None:
        shutter_rev = _RecordingNode()
        shutter_fwd = _RecordingNode()
        features = {
            "Specim": {
                "MotorShutter": {
                    "MotorShutter_PulseRev": shutter_rev,
                    "MotorShutter_PulseFwd": shutter_fwd,
                }
            }
        }
        worker = object.__new__(FX17CameraStreamWorker)
        worker._commands = queue.Queue()
        worker._messages = queue.Queue()
        worker._active_calibration = None
        worker.dark_ref_path = Path("old-dark.dat")
        worker.white_ref_path = Path("old-white.dat")
        worker._write_reference_files = lambda request, reference: None
        worker._build_pipeline = lambda: self.fail(
            "Pair dark capture must not reload the pipeline"
        )
        request = _CalibrationRequest(
            kind="dark",
            path=Path("staged-dark.dat"),
            frame_count=1,
            settle_frames=0,
            settle_seconds=0.0,
            activate_after_capture=False,
        )
        worker._commands.put(("capture_reference", request))

        shutter_open = worker._apply_commands(
            features,
            shutter_is_open=True,
        )
        original_pipeline = object()
        pipeline, shutter_open, consumed = worker._consume_calibration_line(
            np.full((2, 3), 3, dtype=np.uint16),
            original_pipeline,
            features,
            shutter_is_open=shutter_open,
        )
        messages = worker.poll()

        self.assertIs(pipeline, original_pipeline)
        self.assertTrue(shutter_open)
        self.assertTrue(consumed)
        self.assertEqual(worker.dark_ref_path, Path("staged-dark.dat"))
        self.assertEqual(messages[-1].kind, "calibration_complete")
        self.assertFalse(messages[-1].calibration_activated)
        self.assertIsNone(messages[-1].error)


class CameraWorkerMessageTests(unittest.TestCase):
    def test_startup_failure_uses_shared_stream_message(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            missing = Path(directory) / "missing.cti"
            worker = FX17CameraStreamWorker(
                cti_path=missing,
                dark_ref_path=missing,
                white_ref_path=missing,
                wavelength_hdr_path=missing,
                bands=4,
                width=5,
            )
            deadline = time.monotonic() + 2.0
            messages: list[StreamMessage] = []
            while time.monotonic() < deadline and not messages:
                time.sleep(0.02)
                messages = worker.poll()
            worker.close()

        self.assertEqual(len(messages), 1)
        self.assertIsInstance(messages[0], StreamMessage)
        self.assertEqual(messages[0].kind, "error")
        self.assertIn("FileNotFoundError", messages[0].error)


class CameraProcessingDecouplingTests(unittest.TestCase):
    @staticmethod
    def make_processor(
        pipeline: _SlowPipeline,
        *,
        queue_capacity_lines: int = 256,
    ) -> tuple[_LineProcessingWorker, queue.Queue[StreamMessage]]:
        messages: queue.Queue[StreamMessage] = queue.Queue()
        processor = _LineProcessingWorker(
            pipeline=pipeline,
            messages=messages,
            chunk_lines=20,
            queue_capacity_lines=queue_capacity_lines,
            pipeline_config={
                "detector_percentile_low": 1.0,
                "detector_percentile_high": 99.0,
            },
            diagnostics=AcquisitionDiagnosticsLogger(None, enabled=False),
        )
        processor.set_state(analysis_enabled=True, reset=True)
        return processor, messages

    def test_slow_twenty_line_processing_does_not_block_line_submission(
        self,
    ) -> None:
        pipeline = _SlowPipeline(delay_seconds=0.14)
        processor, _ = self.make_processor(pipeline)
        line = np.zeros((4, 5), dtype=np.uint16)

        started = time.perf_counter()
        accepted = [
            processor.submit_line(
                line.copy(),
                frame_id=index,
                timestamp_ns=index * 5_771_000,
            )
            for index in range(60)
        ]
        submission_seconds = time.perf_counter() - started
        processor.close(timeout=2.0, drain=True)

        self.assertTrue(all(accepted))
        self.assertLess(submission_seconds, 0.10)
        self.assertEqual(processor.queue_drops, 0)
        self.assertEqual(
            [frame for block, _ in pipeline.blocks for frame in block],
            list(range(60)),
        )
        self.assertEqual([len(block) for block, _ in pipeline.blocks], [20] * 3)

    def test_real_rate_burst_is_buffered_while_processing_is_over_budget(
        self,
    ) -> None:
        pipeline = _SlowPipeline(delay_seconds=0.14)
        processor, _ = self.make_processor(
            pipeline,
            queue_capacity_lines=256,
        )
        line = np.zeros((4, 5), dtype=np.uint16)
        interval = 1.0 / 173.26686096191406

        for index in range(120):
            self.assertTrue(
                processor.submit_line(
                    line.copy(),
                    frame_id=index,
                    timestamp_ns=int(index * interval * 1_000_000_000),
                )
            )
            time.sleep(interval)
        processor.close(timeout=3.0, drain=True)

        self.assertEqual(processor.queue_drops, 0)
        self.assertEqual(len(pipeline.blocks), 6)
        self.assertEqual(
            [frame for block, _ in pipeline.blocks for frame in block],
            list(range(120)),
        )

    def test_full_processing_queue_counts_drops_instead_of_blocking_fetch(
        self,
    ) -> None:
        pipeline = _SlowPipeline(delay_seconds=0.2)
        processor, _ = self.make_processor(
            pipeline,
            queue_capacity_lines=20,
        )
        line = np.zeros((4, 5), dtype=np.uint16)

        started = time.perf_counter()
        for index in range(200):
            processor.submit_line(
                line,
                frame_id=index,
                timestamp_ns=index,
            )
        submission_seconds = time.perf_counter() - started
        processor.close(timeout=2.0, drain=False)

        self.assertLess(submission_seconds, 0.10)
        self.assertGreater(processor.queue_drops, 0)


class AcquisitionDiagnosticsTests(unittest.TestCase):
    def test_records_frame_gap_queue_and_processing_summary(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            logger = AcquisitionDiagnosticsLogger(
                directory,
                expected_line_rate_hz=100.0,
                session_label="TEST",
            )
            for frame_id in (100, 101, 104):
                logger.record_frame(
                    frame_id=frame_id,
                    camera_timestamp_ns=frame_id * 10_000_000,
                    fetch_wait_ms=5.0,
                    copy_ms=0.5,
                    processing_queue_depth=7,
                    processing_queue_drops=2,
                )
            logger.record_timeout(consecutive_timeouts=1)
            logger.record_block(
                block_lines=20,
                processing_ms=250.0,
                processing_queue_depth=9,
                processing_queue_drops=2,
            )
            paths = logger.paths
            logger.close()
            assert paths is not None
            summary = json.loads(
                paths.summary_path.read_text(encoding="utf-8")
            )
            csv_text = paths.csv_path.read_text(encoding="utf-8-sig")

        self.assertEqual(summary["frames_fetched"], 3)
        self.assertEqual(summary["frame_id_gap_events"], 1)
        self.assertEqual(summary["missing_camera_frames"], 2)
        self.assertEqual(summary["processing_queue_max_depth"], 9)
        self.assertEqual(summary["processing_queue_drops"], 2)
        self.assertEqual(summary["processing_blocks_over_budget"], 1)
        self.assertIn("camera_delta_ms", csv_text.splitlines()[0])


if __name__ == "__main__":
    unittest.main()
