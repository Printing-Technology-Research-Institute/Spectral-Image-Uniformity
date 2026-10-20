from __future__ import annotations

import tempfile
import unittest
from unittest.mock import patch
from pathlib import Path

from simulate_stream_detection import resolve_workspace_file
from simulation_app import (
    _shared_pipeline_settings,
    exposure_us_from_settings,
    load_settings,
    stream_error_message,
)


class StreamPathTests(unittest.TestCase):
    def test_relocates_a_missing_absolute_provenance_path_by_filename(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            local_directory = Path(directory)
            relocated = local_directory / "50u_1.hdr"
            relocated.write_text("ENVI", encoding="utf-8")

            resolved = resolve_workspace_file(
                r"C:\Users\old-user\Desktop\產學\20260528_導電材料\50u_1.hdr",
                local_directory,
            )

            self.assertEqual(resolved, relocated)

    def test_keeps_an_existing_recorded_path(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            recorded = Path(directory) / "50u_1.raw"
            recorded.write_bytes(b"\x00\x01")

            resolved = resolve_workspace_file(recorded, Path(directory) / "other")

            self.assertEqual(resolved, recorded)

    def test_exposure_ms_takes_precedence_and_converts_to_us(self) -> None:
        self.assertEqual(
            exposure_us_from_settings(
                {"exposure_ms": 1.25, "exposure_us": 9999}
            ),
            1250.0,
        )
        self.assertEqual(
            exposure_us_from_settings({"exposure_us": 5000}),
            5000.0,
        )

    def test_missing_runtime_settings_receive_recording_defaults(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            settings_path = Path(directory) / "settings.json"
            settings_path.write_text('{"stream_source": "camera"}', encoding="utf-8")

            with patch("simulation_app.SETTINGS_PATH", settings_path):
                settings = load_settings()

        self.assertEqual(settings["sample_start_threshold"], 0.05)
        self.assertEqual(settings["sample_end_threshold"], 0.04)
        self.assertEqual(settings["max_sample_lines"], 1200)
        self.assertEqual(settings["camera_processing_queue_lines"], 2048)
        self.assertFalse(settings["save_analysis_captures"])
        self.assertEqual(settings["analysis_capture_limit_per_run"], 0)
        self.assertTrue(settings["save_sample_records"])
        self.assertEqual(settings["sample_record_dir"], "../../sample_records")

    def test_missing_boundary_thresholds_use_thin_sample_defaults(self) -> None:
        shared = _shared_pipeline_settings({})

        self.assertEqual(shared["start_threshold"], 0.05)
        self.assertEqual(shared["end_threshold"], 0.04)
        self.assertEqual(shared["max_sample_lines"], 1200)

    def test_camera_timeout_message_explains_how_to_recover(self) -> None:
        message = stream_error_message(
            "TimeoutError: FX17 timed out 20 consecutive times."
        )

        self.assertIn("未收到影像 buffer", message)
        self.assertIn("重新啟動 FX17 電源", message)

    def test_camera_discovery_message_mentions_connection_checks(self) -> None:
        message = stream_error_message(
            "RuntimeError: No GenTL camera was discovered after 12 attempts."
        )

        self.assertIn("找不到 FX17 相機", message)
        self.assertIn("其他取像程式", message)

    def test_multiple_fx17_message_refuses_ambiguous_auto_selection(self) -> None:
        message = stream_error_message(
            "RuntimeError: Multiple FX17 cameras were discovered"
        )

        self.assertIn("偵測到多台 FX17", message)
        self.assertIn("未自動連線", message)


if __name__ == "__main__":
    unittest.main()
