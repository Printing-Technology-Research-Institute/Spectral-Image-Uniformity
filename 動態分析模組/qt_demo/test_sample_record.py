from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path

import numpy as np

from sample_record import (
    read_envi_wavelengths,
    update_sample_record,
    write_sample_record,
    write_sample_record_bundle,
    write_sample_record_pair,
)

CAE_DIR = Path(__file__).resolve().parents[2] / "CAE訓練"
if str(CAE_DIR) not in sys.path:
    sys.path.insert(0, str(CAE_DIR))
from auto_sam_mask import load_envi_bil


class SampleRecordTests(unittest.TestCase):
    def test_writes_round_trip_envi_bil_and_parameters(self) -> None:
        cube = (np.arange(3 * 4 * 2, dtype=np.float32) / 10.0).reshape(3, 4, 2)
        with tempfile.TemporaryDirectory() as directory:
            stem = write_sample_record(
                directory,
                stem_name="20260812_180000_FX17_0001",
                sample_id="FX17_0001",
                cube=cube,
                acquisition_metadata={"exposure_ms": 3.0, "line_rate_hz": 193.5},
                wavelengths_nm=[1000.0, 1010.0],
            )

            raw = np.fromfile(stem.with_suffix(".raw"), dtype="<f4")
            restored = raw.reshape(3, 2, 4).transpose(0, 2, 1).copy()
            np.testing.assert_array_equal(restored, cube)
            loaded, wavelengths, resolved = load_envi_bil(
                stem.with_suffix(".hdr"), trim_bands=0
            )
            np.testing.assert_array_equal(loaded, cube)
            np.testing.assert_array_equal(wavelengths, [1000.0, 1010.0])
            self.assertEqual(resolved["bands"], 2)

            header = stem.with_suffix(".hdr").read_text(encoding="utf-8")
            self.assertIn("samples = 4", header)
            self.assertIn("lines = 3", header)
            self.assertIn("bands = 2", header)
            self.assertIn("data type = 4", header)
            self.assertIn("interleave = bil", header)
            self.assertIn("1000.000000", header)

            metadata = json.loads(
                stem.with_suffix(".json").read_text(encoding="utf-8")
            )
            self.assertEqual(metadata["sample_id"], "FX17_0001")
            self.assertEqual(metadata["parameters"]["exposure_ms"], 3.0)
            self.assertEqual(metadata["envi"]["shape"], [3, 4, 2])
            self.assertEqual(metadata["analysis"]["status"], "pending")

            update_sample_record(
                stem,
                {"status": "analyzed", "analysis": {"uniformity_percent": 22.8}},
            )
            updated = json.loads(
                stem.with_suffix(".json").read_text(encoding="utf-8")
            )
            self.assertEqual(updated["status"], "analyzed")
            self.assertEqual(updated["analysis"]["uniformity_percent"], 22.8)

    def test_reads_and_trims_source_wavelengths(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            header = Path(directory) / "source.hdr"
            header.write_text(
                "ENVI\nbands = 6\nwavelength = {900, 910, 920, 930, 940, 950}\n",
                encoding="utf-8",
            )
            values = read_envi_wavelengths(
                header,
                trim_bands=1,
                expected_bands=4,
            )
            self.assertEqual(values, [910.0, 920.0, 930.0, 940.0])

    def test_writes_paired_uncalibrated_and_calibrated_folders(self) -> None:
        raw_cube = np.arange(3 * 4 * 6, dtype=np.uint16).reshape(3, 4, 6)
        calibrated_cube = raw_cube[:, :, 1:-1].astype(np.float32) / 100.0
        with tempfile.TemporaryDirectory() as directory:
            uncalibrated, calibrated = write_sample_record_pair(
                directory,
                stem_name="20260812_190000_FX17_0002",
                sample_id="FX17_0002",
                uncalibrated_cube=raw_cube,
                calibrated_cube=calibrated_cube,
                acquisition_metadata={"exposure_ms": 3.0},
                uncalibrated_wavelengths_nm=[900, 910, 920, 930, 940, 950],
                calibrated_wavelengths_nm=[910, 920, 930, 940],
            )

            self.assertEqual(uncalibrated.parent.name, "uncalibrated")
            self.assertEqual(calibrated.parent.name, "calibrated")
            raw = np.fromfile(uncalibrated.with_suffix(".raw"), dtype="<u2")
            restored = raw.reshape(3, 6, 4).transpose(0, 2, 1).copy()
            np.testing.assert_array_equal(restored, raw_cube)
            uncalibrated_header = uncalibrated.with_suffix(".hdr").read_text(
                encoding="utf-8"
            )
            calibrated_header = calibrated.with_suffix(".hdr").read_text(
                encoding="utf-8"
            )
            self.assertIn("bands = 6", uncalibrated_header)
            self.assertIn("data type = 12", uncalibrated_header)
            self.assertNotIn("reflectance scale factor", uncalibrated_header)
            self.assertIn("bands = 4", calibrated_header)
            self.assertIn("data type = 4", calibrated_header)

            uncalibrated_metadata = json.loads(
                uncalibrated.with_suffix(".json").read_text(encoding="utf-8")
            )
            calibrated_metadata = json.loads(
                calibrated.with_suffix(".json").read_text(encoding="utf-8")
            )
            self.assertEqual(uncalibrated_metadata["data_kind"], "uncalibrated")
            self.assertEqual(calibrated_metadata["data_kind"], "calibrated")
            self.assertEqual(
                Path(uncalibrated_metadata["paired_record"]["hdr"]),
                calibrated.with_suffix(".hdr"),
            )
            self.assertEqual(
                Path(calibrated_metadata["paired_record"]["hdr"]),
                uncalibrated.with_suffix(".hdr"),
            )

    def test_writes_capture_time_bundle_into_one_sample_folder(self) -> None:
        raw_cube = np.arange(2 * 3 * 4, dtype=np.uint16).reshape(2, 3, 4)
        calibrated_cube = raw_cube[:, :, 1:-1].astype(np.float32) / 100.0
        with tempfile.TemporaryDirectory() as directory:
            uncalibrated, calibrated = write_sample_record_bundle(
                directory,
                file_stem="20260818_091234_123456",
                sample_id="FX17_0001",
                uncalibrated_cube=raw_cube,
                calibrated_cube=calibrated_cube,
                uncalibrated_wavelengths_nm=[900, 910, 920, 930],
                calibrated_wavelengths_nm=[910, 920],
            )

            self.assertEqual(uncalibrated.parent, Path(directory).resolve())
            self.assertEqual(calibrated.parent, Path(directory).resolve())
            self.assertEqual(
                uncalibrated.name,
                "20260818_091234_123456_未校正",
            )
            self.assertEqual(calibrated.name, "20260818_091234_123456_校正")
            for stem in (uncalibrated, calibrated):
                for suffix in (".raw", ".hdr", ".json"):
                    self.assertTrue(stem.with_suffix(suffix).is_file())


if __name__ == "__main__":
    unittest.main()
