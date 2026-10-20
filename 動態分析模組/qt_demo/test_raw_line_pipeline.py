from __future__ import annotations

import unittest

import numpy as np

from raw_line_pipeline import RawLinePipeline
from sample_boundary_detector import DetectorConfig, LineCalibrator


class RawLinePipelineModeTests(unittest.TestCase):
    @staticmethod
    def make_pipeline(
        *,
        spatial_roi_start: int = 0,
        spatial_roi_end: int | None = None,
    ) -> RawLinePipeline:
        calibrator = LineCalibrator(
            np.zeros((4, 6), dtype=np.float32),
            np.full((4, 6), 10.0, dtype=np.float32),
            trim_bands=0,
            spatial_roi_start=spatial_roi_start,
            spatial_roi_end=spatial_roi_end,
        )
        return RawLinePipeline(
            calibrator=calibrator,
            detector_config=DetectorConfig(
                material_pixel_threshold=0.08,
                start_threshold=0.5,
                end_threshold=0.2,
                start_confirm_lines=2,
                end_confirm_lines=2,
                min_sample_lines=2,
                max_sample_lines=20,
            ),
        )

    def test_preview_continues_when_sample_detection_is_disabled(self) -> None:
        pipeline = self.make_pipeline()
        raw = np.full((4, 4, 6), 5.0, dtype=np.float32)

        messages = pipeline.process_block(
            raw,
            sample_id="calibration",
            detect_samples=False,
        )

        self.assertEqual([message.kind for message in messages], ["preview"])
        self.assertIsNotNone(messages[0].detector_frame)
        self.assertIsNotNone(messages[0].display_lines)

    def test_sample_detection_can_be_reenabled_after_reset(self) -> None:
        pipeline = self.make_pipeline()
        high = np.full((4, 4, 6), 5.0, dtype=np.float32)
        low = np.zeros((3, 4, 6), dtype=np.float32)

        pipeline.process_block(
            high,
            sample_id="preview-only",
            detect_samples=False,
        )
        pipeline.reset()
        messages = pipeline.process_block(
            high,
            sample_id="circle",
            detect_samples=True,
        )
        messages += pipeline.process_block(
            low,
            sample_id="circle",
            detect_samples=True,
        )

        kinds = [message.kind for message in messages]
        self.assertIn("sample_start", kinds)
        self.assertIn("sample_end", kinds)

    def test_completed_cube_preserves_pixels_outside_detection_roi(self) -> None:
        pipeline = self.make_pipeline(
            spatial_roi_start=1,
            spatial_roi_end=5,
        )
        high = np.full((3, 4, 6), 5, dtype=np.uint16)
        low = np.zeros((2, 4, 6), dtype=np.uint16)

        messages = pipeline.process_block(
            high,
            sample_id="roi-sample",
            detect_samples=True,
        )
        messages += pipeline.process_block(
            low,
            sample_id="roi-sample",
            detect_samples=True,
        )
        completed = next(
            message for message in messages if message.kind == "sample_end"
        )

        self.assertEqual(completed.cube.shape, (3, 6, 4))
        np.testing.assert_allclose(completed.cube, 0.5, atol=1e-6)
        self.assertEqual(completed.raw_cube.shape, (3, 6, 4))
        self.assertEqual(completed.raw_cube.dtype, np.uint16)
        np.testing.assert_array_equal(completed.raw_cube, 5)

    def test_frame_gap_metadata_does_not_block_completed_cube(self) -> None:
        pipeline = self.make_pipeline()
        high = np.full((3, 4, 6), 5, dtype=np.uint16)
        low = np.zeros((2, 4, 6), dtype=np.uint16)
        messages = pipeline.process_block(
            high,
            sample_id="gap-sample",
            frame_ids=[10, 11, 13],
            timestamps_ns=[0, 6_000_000, 18_000_000],
        )
        messages += pipeline.process_block(
            low,
            sample_id="gap-sample",
            frame_ids=[14, 15],
            timestamps_ns=[24_000_000, 30_000_000],
        )

        completed = next(
            message for message in messages if message.kind == "sample_end"
        )
        self.assertIsNotNone(completed.cube)
        self.assertEqual(completed.acquisition_integrity["status"], "incomplete")
        self.assertEqual(
            completed.acquisition_integrity["frame_id_missing_count"], 1
        )


if __name__ == "__main__":
    unittest.main()
