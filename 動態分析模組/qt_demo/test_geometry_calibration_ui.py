from __future__ import annotations

import os
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

import numpy as np

os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")

from PySide6.QtWidgets import QApplication, QMessageBox

from simulation_app import MainWindow, calibration_pair_paths
from stream_source import StreamMessage


class _FakeAnalysisWorker:
    def __init__(self, **kwargs) -> None:
        self.closed = False

    def poll(self) -> list:
        return []

    def submit_cube(self, sample_id, cube) -> bool:
        return True

    def close(self, timeout: float = 2.0) -> None:
        self.closed = True


class _FakeStreamWorker:
    def __init__(self) -> None:
        self.resume_count = 0
        self.pause_count = 0
        self.reset_count = 0
        self.analysis_states: list[bool] = []
        self.line_rate_requests = 0
        self.line_rate_sets: list[float] = []
        self.capture_requests: list[tuple[str, Path, bool]] = []

    def resume(self) -> None:
        self.resume_count += 1

    def pause(self) -> None:
        self.pause_count += 1

    def set_analysis_enabled(self, enabled: bool) -> None:
        self.analysis_states.append(bool(enabled))

    def reset_analysis(self) -> None:
        self.reset_count += 1

    def request_line_rate(self) -> None:
        self.line_rate_requests += 1

    def set_line_rate_hz(self, line_rate_hz: float) -> None:
        self.line_rate_sets.append(float(line_rate_hz))

    def capture_reference(
        self,
        kind: str,
        path: str | Path,
        *,
        frame_count: int | None = None,
        activate_after_capture: bool = True,
    ) -> None:
        del frame_count
        self.capture_requests.append(
            (kind, Path(path), bool(activate_after_capture))
        )

    def poll(self) -> list:
        return []

    def close(self, timeout: float = 2.0) -> None:
        pass


class GeometryCalibrationUiTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.app = QApplication.instance() or QApplication([])

    def test_geometry_mode_keeps_stream_running_and_only_toggles_analysis(self) -> None:
        stream = _FakeStreamWorker()
        with patch("simulation_app.LiveAnalysisWorker", _FakeAnalysisWorker):
            window = MainWindow(stream_worker=stream)
        with patch.object(
            QMessageBox,
            "question",
            return_value=QMessageBox.StandardButton.Ok,
        ):
            window._toggle_geometry_calibration()

        self.assertTrue(window.geometry_calibration_active)
        self.assertEqual(stream.resume_count, 1)
        self.assertEqual(stream.pause_count, 0)
        self.assertEqual(stream.reset_count, 1)
        self.assertFalse(stream.analysis_states[-1])
        self.assertEqual(stream.line_rate_requests, 1)

        window._handle_line_rate_message(
            StreamMessage(
                kind="line_rate_report",
                line_rate_hz=300.0,
                line_rate_min_hz=2.0,
                line_rate_max_hz=330.0,
                line_rate_control_enabled=False,
            )
        )

        self.assertEqual(
            window.geometry_calibration_phase,
            "capturing_standard",
        )
        self.assertTrue(stream.analysis_states[-1])

        window._cancel_geometry_calibration()

        self.assertFalse(window.geometry_calibration_active)
        self.assertEqual(stream.pause_count, 0)
        self.assertFalse(stream.analysis_states[-1])
        window.close()

    def test_calibration_pair_name_can_be_edited_from_a_suggested_name(self) -> None:
        self.assertEqual(
            calibration_pair_paths("my_reference_Ref-Dark.dat"),
            {
                "dark": Path("my_reference_Ref-Dark.dat"),
                "white": Path("my_reference_Ref-White.dat"),
            },
        )
        with tempfile.TemporaryDirectory() as temporary_directory:
            directory = Path(temporary_directory)
            settings = {
                "stream_source": "camera",
                "camera_serial_number": "SERIAL-42",
                "exposure_ms": 3.0,
                "dark_ref_path": str(directory / "old_dark.dat"),
            }
            stream = _FakeStreamWorker()
            with patch(
                "simulation_app.LiveAnalysisWorker",
                _FakeAnalysisWorker,
            ), patch("simulation_app.load_settings", return_value=settings):
                window = MainWindow(stream_worker=stream)

            chosen = directory / "onsite_reference.dat"
            with patch(
                "simulation_app.QFileDialog.getSaveFileName",
                return_value=(str(chosen), "Calibration data (*.dat)"),
            ) as save_dialog, patch.object(
                QMessageBox,
                "question",
                return_value=QMessageBox.StandardButton.Ok,
            ):
                window._start_calibration_pair()

            suggested_path = Path(save_dialog.call_args.args[2])
            self.assertEqual(suggested_path.parent, directory)
            self.assertTrue(
                suggested_path.name.startswith("FX17_SERIAL-42_3ms_")
            )
            self.assertEqual(
                stream.capture_requests,
                [
                    (
                        "dark",
                        directory / "onsite_reference_Ref-Dark.dat",
                        False,
                    )
                ],
            )
            self.assertEqual(
                window.calibration_pair_paths["white"],
                directory / "onsite_reference_Ref-White.dat",
            )
            window.close()

    def test_circle_updates_camera_line_rate_without_resampling_cube(self) -> None:
        stream = _FakeStreamWorker()
        with patch("simulation_app.LiveAnalysisWorker", _FakeAnalysisWorker):
            window = MainWindow(stream_worker=stream)
        with patch.object(
            QMessageBox,
            "question",
            return_value=QMessageBox.StandardButton.Ok,
        ):
            window._toggle_geometry_calibration()
        window._handle_line_rate_message(
            StreamMessage(
                kind="line_rate_report",
                line_rate_hz=300.0,
                line_rate_min_hz=2.0,
                line_rate_max_hz=330.0,
                line_rate_control_enabled=False,
            )
        )

        lines, width, bands = 360, 200, 12
        y, x = np.ogrid[:lines, :width]
        mask = (
            ((x - width / 2) / 35.0) ** 2
            + ((y - lines / 2) / 105.0) ** 2
        ) <= 1.0
        cube = np.full((lines, width, bands), 0.82, dtype=np.float32)
        cube[mask] = 0.14
        window._accept_geometry_calibration(
            StreamMessage(
                kind="sample_end",
                sample_id="circle_standard",
                cube=cube,
            )
        )

        self.assertEqual(window.geometry_calibration_phase, "applying_line_rate")
        self.assertEqual(len(stream.line_rate_sets), 1)
        requested = stream.line_rate_sets[0]
        self.assertAlmostEqual(requested, 100.0, delta=4.0)

        with patch.object(window, "_persist_settings"), patch.object(
            QMessageBox,
            "information",
        ):
            window._handle_line_rate_message(
                StreamMessage(
                    kind="line_rate_set",
                    line_rate_hz=requested,
                    line_rate_requested_hz=requested,
                    line_rate_min_hz=2.0,
                    line_rate_max_hz=330.0,
                    line_rate_control_enabled=True,
                )
            )

        profile = window.settings["geometry_calibration"]
        self.assertEqual(
            profile["correction_method"],
            "fx17_acquisition_line_rate",
        )
        self.assertEqual(profile["sample_id"], "circle_standard")
        self.assertNotIn("scan_scale", profile)
        self.assertAlmostEqual(profile["applied_line_rate_hz"], requested)
        window.close()

    def test_out_of_range_target_shows_direction_specific_guidance(self) -> None:
        cases = (
            (300.0, 1.2, "降低曝光時間", "最大值"),
            (2.5, 0.5, "提高輸送帶速度", "最小值"),
        )
        for current_hz, multiplier, guidance, limit_name in cases:
            with self.subTest(guidance=guidance):
                stream = _FakeStreamWorker()
                with patch(
                    "simulation_app.LiveAnalysisWorker",
                    _FakeAnalysisWorker,
                ):
                    window = MainWindow(stream_worker=stream)
                window.geometry_calibration_active = True
                window.geometry_calibration_phase = "capturing_standard"
                window.geometry_calibration_current_line_rate_hz = current_hz
                window.geometry_calibration_line_rate_min_hz = 2.0
                window.geometry_calibration_line_rate_max_hz = 330.0
                result = SimpleNamespace(
                    line_rate_multiplier=multiplier,
                    preview=np.zeros((20, 20, 3), dtype=np.uint8),
                )

                with patch(
                    "simulation_app.estimate_geometry_calibration",
                    return_value=result,
                ), patch.object(QMessageBox, "critical") as critical:
                    window._accept_geometry_calibration(
                        StreamMessage(
                            kind="sample_end",
                            sample_id="circle_standard",
                            cube=np.zeros((20, 20, 4), dtype=np.float32),
                        )
                    )

                self.assertEqual(stream.line_rate_sets, [])
                self.assertFalse(window.geometry_calibration_active)
                detail = critical.call_args.args[2]
                self.assertIn(guidance, detail)
                self.assertIn(limit_name, detail)
                self.assertIn("程式未修改曝光時間或 line rate", detail)
                window.close()

    def test_exposure_change_keeps_invalidation_reason_after_restart(self) -> None:
        settings = {
            "exposure_ms": 3.0,
            "geometry_calibration": {
                "valid": True,
                "applied_line_rate_hz": 200.0,
                "exposure_ms": 3.0,
            },
        }
        stream = _FakeStreamWorker()
        with patch(
            "simulation_app.LiveAnalysisWorker",
            _FakeAnalysisWorker,
        ), patch("simulation_app.load_settings", return_value=settings):
            window = MainWindow(stream_worker=stream)

        self.assertFalse(hasattr(window.sidebar, "conveyor_speed"))
        self.assertFalse(hasattr(window.sidebar, "working_distance"))
        with patch.object(window, "_persist_settings"):
            window._set_camera_exposure(4.0)

        profile = window.settings["geometry_calibration"]
        self.assertFalse(profile["valid"])
        self.assertEqual(profile["invalid_reason"], "曝光時間已變更")
        self.assertEqual(
            window.sidebar.geometry_calibration_status.text(),
            "曝光時間已變更，請重新進行推掃取像參數校正",
        )
        window.close()

        restarted_stream = _FakeStreamWorker()
        with patch(
            "simulation_app.LiveAnalysisWorker",
            _FakeAnalysisWorker,
        ), patch("simulation_app.load_settings", return_value=settings):
            restarted = MainWindow(stream_worker=restarted_stream)
        self.assertEqual(
            restarted.sidebar.geometry_calibration_status.text(),
            "曝光時間已變更，請重新進行推掃取像參數校正",
        )
        restarted.close()

    def test_connected_camera_identity_is_shown_and_saved(self) -> None:
        settings = {
            "stream_source": "camera",
            "camera_serial_number": "OLD-SERIAL",
            "geometry_calibration": {
                "valid": True,
                "camera_serial_number": "OLD-SERIAL",
                "applied_line_rate_hz": 200.0,
            },
        }
        stream = _FakeStreamWorker()
        with patch(
            "simulation_app.LiveAnalysisWorker",
            _FakeAnalysisWorker,
        ), patch("simulation_app.load_settings", return_value=settings):
            window = MainWindow(stream_worker=stream)

        with patch.object(window, "_persist_settings"):
            window._handle_camera_connected(
                StreamMessage(
                    kind="camera_connected",
                    camera_model="FX17e",
                    camera_serial_number="NEW-SERIAL",
                    camera_vendor="Specim",
                    camera_auto_selected=True,
                )
            )

        identity = window.sidebar.camera_identity_status.text()
        self.assertIn("FX17e", identity)
        self.assertIn("NEW-SERIAL", identity)
        self.assertIn("自動選擇唯一的 FX17", identity)
        self.assertEqual(window.settings["camera_serial_number"], "NEW-SERIAL")
        profile = window.settings["geometry_calibration"]
        self.assertFalse(profile["valid"])
        self.assertEqual(profile["invalid_reason"], "相機序號已變更")
        window.close()


if __name__ == "__main__":
    unittest.main()
