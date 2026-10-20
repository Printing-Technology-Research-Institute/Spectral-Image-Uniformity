from __future__ import annotations

import unittest

import numpy as np

from sample_boundary_detector import (
    DetectorConfig,
    LineCalibrator,
    SampleBoundaryDetector,
)
from replay_backend import RollingLineBuffer


class SampleBoundaryDetectorTests(unittest.TestCase):
    def make_detector(
        self,
        collect_cube: bool = False,
        collect_raw_cube: bool = False,
    ) -> SampleBoundaryDetector:
        return SampleBoundaryDetector(
            DetectorConfig(
                start_threshold=0.6,
                end_threshold=0.3,
                start_confirm_lines=3,
                end_confirm_lines=2,
                min_sample_lines=3,
                max_sample_lines=20,
            ),
            collect_cube=collect_cube,
            collect_raw_cube=collect_raw_cube,
        )

    def test_backdates_confirmed_boundaries(self) -> None:
        detector = self.make_detector(collect_cube=True)
        scores = [0.1, 0.2, 0.7, 0.8, 0.9, 0.7, 0.1, 0.1]
        events = []
        for index, score in enumerate(scores):
            payload = np.full((2, 3), index, dtype=np.float32)
            events.extend(detector.feed(score, payload))

        self.assertEqual([event.kind for event in events], ["sample_start", "sample_end"])
        end = events[-1]
        self.assertEqual(end.sample_start_line, 2)
        self.assertEqual(end.sample_end_line, 5)
        self.assertEqual(end.sample_line_count, 4)
        self.assertEqual(end.cube.shape, (4, 2, 3))
        np.testing.assert_array_equal(end.cube[:, 0, 0], [2, 3, 4, 5])

    def test_collects_matching_uncalibrated_uint16_cube(self) -> None:
        detector = self.make_detector(
            collect_cube=True,
            collect_raw_cube=True,
        )
        events = []
        for index, score in enumerate([0.7, 0.8, 0.9, 0.7, 0.1, 0.1]):
            corrected = np.full((2, 3), index / 10, dtype=np.float32)
            raw = np.full((2, 4), index + 100, dtype=np.uint16)
            events.extend(detector.feed(score, corrected, raw))

        end = events[-1]
        self.assertEqual(end.kind, "sample_end")
        self.assertEqual(end.cube.shape[0], end.raw_cube.shape[0])
        self.assertEqual(end.raw_cube.dtype, np.uint16)
        np.testing.assert_array_equal(end.raw_cube[:, 0, 0], [100, 101, 102, 103])

    def test_hysteresis_keeps_a_single_low_line(self) -> None:
        detector = self.make_detector()
        scores = [0.7, 0.7, 0.7, 0.2, 0.7, 0.1, 0.1]
        events = []
        for score in scores:
            events.extend(detector.feed(score))
        end = events[-1]
        self.assertEqual(end.kind, "sample_end")
        self.assertEqual(end.sample_line_count, 5)
        self.assertEqual(end.sample_start_line, 0)
        self.assertEqual(end.sample_end_line, 4)

    def test_short_detection_is_rejected(self) -> None:
        detector = SampleBoundaryDetector(
            DetectorConfig(
                start_threshold=0.6,
                end_threshold=0.3,
                start_confirm_lines=2,
                end_confirm_lines=2,
                min_sample_lines=5,
                max_sample_lines=20,
            )
        )
        events = []
        for score in [0.7, 0.7, 0.1, 0.1]:
            events.extend(detector.feed(score))
        self.assertEqual(events[-1].kind, "sample_rejected")

    def test_reports_frame_gaps_without_rejecting_sample(self) -> None:
        detector = self.make_detector()
        events = []
        frame_ids = [100, 101, 103, 104, 105, 106]
        timestamps = [0, 6_000_000, 18_000_000, 24_000_000, 30_000_000, 36_000_000]
        for score, frame_id, timestamp_ns in zip(
            [0.7, 0.8, 0.9, 0.7, 0.1, 0.1],
            frame_ids,
            timestamps,
        ):
            events.extend(
                detector.feed(
                    score,
                    frame_id=frame_id,
                    timestamp_ns=timestamp_ns,
                )
            )

        end = events[-1]
        self.assertEqual(end.kind, "sample_end")
        self.assertEqual(end.acquisition_integrity["status"], "incomplete")
        self.assertEqual(end.acquisition_integrity["frame_id_missing_count"], 1)
        self.assertEqual(end.acquisition_integrity["received_lines"], 4)


class RollingLineBufferTests(unittest.TestCase):
    def test_pushes_only_received_lines_into_view(self) -> None:
        buffer = RollingLineBuffer(width=3, viewport_lines=4)
        buffer.push(np.full((2, 3), 10, dtype=np.uint8))
        first = buffer.frame()
        np.testing.assert_array_equal(first[:2], 0)
        np.testing.assert_array_equal(first[2:], 10)

        buffer.push(np.full((3, 3), 20, dtype=np.uint8))
        second = buffer.frame()
        np.testing.assert_array_equal(second[0], 10)
        np.testing.assert_array_equal(second[1:], 20)


class LineCalibratorReferenceTests(unittest.TestCase):
    def test_tolerates_isolated_nonpositive_white_pixels(self) -> None:
        dark = np.zeros((4, 6), dtype=np.float32)
        white = np.full((4, 6), 10, dtype=np.float32)
        white[:, 0] = -1
        calibrator = LineCalibrator(
            dark,
            white,
            trim_bands=0,
        )

        corrected = calibrator.calibrate_block(
            np.full((4, 6), 5, dtype=np.float32)
        )

        self.assertEqual(int(calibrator.valid_columns.sum()), 5)
        np.testing.assert_array_equal(corrected[0, :, 0], 0)
        np.testing.assert_allclose(corrected[0, :, 1:], 0.5, atol=1e-6)

    def test_rejects_reference_with_most_pixels_not_brighter(self) -> None:
        dark = np.full((4, 6), 10, dtype=np.float32)
        white = np.full((4, 6), 5, dtype=np.float32)
        white[:, :2] = 20

        with self.assertRaisesRegex(ValueError, "at least half"):
            LineCalibrator(dark, white, trim_bands=0)

    def test_spatial_roi_excludes_edges_from_score_but_preserves_cube(self) -> None:
        calibrator = LineCalibrator(
            np.zeros((4, 8), dtype=np.float32),
            np.full((4, 8), 10, dtype=np.float32),
            trim_bands=0,
            material_pixel_threshold=0.08,
            spatial_roi_start=2,
            spatial_roi_end=6,
        )
        raw = np.zeros((4, 8), dtype=np.float32)
        raw[:, [0, 1, 6, 7]] = 10.0
        corrected = calibrator.calibrate_block(raw)

        self.assertEqual(float(calibrator.line_scores(corrected)[0]), 0.0)
        np.testing.assert_allclose(corrected[0, :, :2], 1.0, atol=1e-6)
        np.testing.assert_allclose(corrected[0, :, 6:], 1.0, atol=1e-6)
        np.testing.assert_allclose(
            calibrator.model_line(corrected[0])[:2], 1.0, atol=1e-6
        )
        np.testing.assert_allclose(
            calibrator.model_line(corrected[0])[6:], 1.0, atol=1e-6
        )

        raw[:, 3:5] = 10.0
        corrected = calibrator.calibrate_block(raw)
        self.assertEqual(float(calibrator.line_scores(corrected)[0]), 0.5)
        self.assertEqual(calibrator.metadata()["spatial_roi_width"], 4)

    def test_rejects_spatial_roi_outside_detector_width(self) -> None:
        dark = np.zeros((4, 8), dtype=np.float32)
        white = np.full((4, 8), 10, dtype=np.float32)

        with self.assertRaisesRegex(ValueError, "spatial_roi"):
            LineCalibrator(
                dark,
                white,
                trim_bands=0,
                spatial_roi_start=2,
                spatial_roi_end=9,
            )


if __name__ == "__main__":
    unittest.main()
