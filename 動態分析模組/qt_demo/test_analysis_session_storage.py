from __future__ import annotations

import tempfile
import unittest
from datetime import datetime
from pathlib import Path

from openpyxl import load_workbook

from analysis_session_storage import AnalysisSessionStorage


class AnalysisSessionStorageTests(unittest.TestCase):
    def test_creates_sample_folder_and_updates_excel_row(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            dark = root / "dark.dat"
            white = root / "white.dat"
            dark.write_bytes(b"dark")
            white.write_bytes(b"white")
            started = datetime(2026, 8, 18, 9, 0, 0)
            storage = AnalysisSessionStorage(
                root / "output",
                started_at=started,
                dark_ref_path=dark,
                white_ref_path=white,
            )
            capture_start = datetime(2026, 8, 18, 9, 12, 34, 123456)
            target = storage.register_sample(
                sample_id="FX17_0001",
                capture_started_at=capture_start,
                capture_ended_at=datetime(2026, 8, 18, 9, 12, 36, 654321),
                acquisition_metadata={
                    "acquisition": {
                        "exposure_ms": 3.0,
                        "line_rate_hz": 173.267,
                    },
                    "sample_boundary": {"sample_line_count": 420},
                },
            )
            storage.complete_sample(
                target.record_id,
                uniformity=12.3,
                resistance_ohm=3.04,
                sample_lines=420,
                elapsed_seconds=0.75,
            )
            storage.finish(datetime(2026, 8, 18, 9, 20, 0))

            self.assertEqual(target.file_stem, "20260818_091234_123456")
            self.assertEqual(target.sample_dir.name, "FX17_0001_20260818_091234_123456")
            self.assertTrue(target.sample_dir.is_dir())
            self.assertTrue(storage.workbook_path.is_file())
            self.assertTrue((storage.calibration_dir / dark.name).is_file())
            self.assertTrue((storage.calibration_dir / white.name).is_file())

            workbook = load_workbook(storage.workbook_path, data_only=False)
            records = workbook["檢測紀錄"]
            self.assertEqual(records["D2"].value, "20260818_091234_123456")
            self.assertEqual(records["E2"].value, "FX17_0001")
            self.assertAlmostEqual(records["F2"].value, 12.3)
            self.assertAlmostEqual(records["G2"].value, 3.04)
            self.assertAlmostEqual(records["H2"].value, 3.0)
            self.assertAlmostEqual(records["I2"].value, 173.267)
            self.assertEqual(records["Q2"].value, "完成")
            self.assertEqual(records.freeze_panes, "A2")
            self.assertEqual(workbook["工作階段資訊"]["B3"].value, datetime(2026, 8, 18, 9, 20, 0))


if __name__ == "__main__":
    unittest.main()
