from __future__ import annotations

import unittest
import json
import tempfile
from pathlib import Path
from types import SimpleNamespace

import numpy as np

from live_analysis import (
    LiveModelAnalyzer,
    _keep_largest_8_connected_component,
    _keep_significant_components,
    find_label_mask_in_spatial_roi,
    save_annotated_heatmap,
    spatial_roi_bounds,
)


class SpatialRoiAnalysisTests(unittest.TestCase):
    def test_runs_automatic_masking_only_on_roi_crop(self) -> None:
        cube = np.zeros((5, 8, 3), dtype=np.float32)
        seen_shapes: list[tuple[int, ...]] = []

        def fake_find_label_mask(roi_cube, args):
            seen_shapes.append(roi_cube.shape)
            return np.ones(roi_cube.shape[:2], dtype=bool), {"label_pixels": 20}

        mask, meta = find_label_mask_in_spatial_roi(
            cube,
            fake_find_label_mask,
            SimpleNamespace(),
            start=2,
            end=6,
        )

        self.assertEqual(seen_shapes, [(5, 4, 3)])
        self.assertFalse(mask[:, :2].any())
        self.assertTrue(mask[:, 2:6].all())
        self.assertFalse(mask[:, 6:].any())
        self.assertEqual(meta["spatial_roi_width"], 4)

    def test_rejects_roi_beyond_cube_width(self) -> None:
        with self.assertRaisesRegex(ValueError, "spatial_roi"):
            spatial_roi_bounds(8, 2, 9)

    def test_mask_cleanup_removes_tiny_islands(self) -> None:
        mask = np.zeros((30, 30), dtype=bool)
        mask[5:25, 8:22] = True
        mask[1, 1] = True
        mask[28, 28] = True

        cleaned, meta = _keep_significant_components(mask)

        self.assertEqual(int(cleaned.sum()), 280)
        self.assertEqual(meta["components_before_cleanup"], 3)
        self.assertEqual(meta["components_after_cleanup"], 1)

    def test_largest_component_removes_every_smaller_component(self) -> None:
        mask = np.zeros((30, 30), dtype=bool)
        mask[5:25, 8:22] = True
        mask[1:3, 1:3] = True
        mask[28, 28] = True

        cleaned, meta = _keep_largest_8_connected_component(mask)

        self.assertEqual(int(cleaned.sum()), 280)
        self.assertEqual(meta["components_before_largest"], 3)
        self.assertEqual(meta["component_sizes_desc"], [280, 4, 1])
        self.assertEqual(meta["removed_component_pixels"], 5)
        self.assertEqual(
            meta["mask_strategy_final"],
            "largest_8_connected_only",
        )

    def test_direct_mask_also_keeps_only_largest_component(self) -> None:
        cube = np.zeros((30, 30, 3), dtype=np.float32)

        def fake_find_label_mask(roi_cube, args):
            mask = np.zeros(roi_cube.shape[:2], dtype=bool)
            mask[5:25, 8:22] = True
            mask[1, 1] = True
            mask[28, 28] = True
            return mask, {
                "sample_pixels": int(mask.size),
                "label_pixels": int(mask.sum()),
            }

        cleaned, meta = find_label_mask_in_spatial_roi(
            cube,
            fake_find_label_mask,
            SimpleNamespace(),
        )

        self.assertEqual(int(cleaned.sum()), 280)
        self.assertFalse(cleaned[1, 1])
        self.assertFalse(cleaned[28, 28])
        self.assertEqual(meta["mask_strategy"], "direct")
        self.assertEqual(
            meta["mask_strategy_final"],
            "largest_8_connected_only",
        )

    def test_diagnostic_capture_saves_cube_mask_and_metrics(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            analyzer = LiveModelAnalyzer.__new__(LiveModelAnalyzer)
            analyzer.capture_dir = Path(directory)
            analyzer.capture_limit = 1
            analyzer.capture_count = 0
            analyzer.spatial_roi_start = 1
            analyzer.spatial_roi_end = 4
            analyzer.calibration_p99 = 0.25
            cube = np.full((3, 5, 2), 0.5, dtype=np.float32)

            stem = analyzer._capture_input("FX17/0001", cube)
            self.assertIsNotNone(stem)
            np.testing.assert_array_equal(np.load(stem.with_suffix(".npy")), cube)

            analyzer._capture_analysis(
                stem,
                label_mask=np.ones((3, 5), dtype=bool),
                residual=np.full((3, 5), 0.1, dtype=np.float32),
                heatmap=np.zeros((3, 5, 3), dtype=np.uint8),
                uniformity=60.0,
                resistance_ohm=4.2,
                mean_residual=0.1,
                p95_residual=0.2,
                hot_area_ratio=0.0,
                mask_meta={"label_pixels": 15},
            )

            metadata = json.loads(
                stem.with_suffix(".json").read_text(encoding="utf-8")
            )
            self.assertEqual(metadata["spatial_roi_start"], 1)
            self.assertEqual(metadata["spatial_roi_end"], 4)
            self.assertEqual(metadata["mask"]["label_pixels"], 15)
            self.assertTrue(
                stem.with_name(stem.name + "_analysis.npz").is_file()
            )

    def test_saves_heatmap_with_metric_header(self) -> None:
        import cv2

        heatmap = np.zeros((40, 80, 3), dtype=np.uint8)
        heatmap[:, :, 0] = 255
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "20260818_091234_123456_殘差熱圖.png"
            saved = save_annotated_heatmap(
                path,
                heatmap,
                uniformity=12.3,
                resistance_ohm=3.04,
            )
            decoded = cv2.imdecode(
                np.fromfile(saved, dtype=np.uint8),
                cv2.IMREAD_COLOR,
            )
            self.assertIsNotNone(decoded)
            self.assertGreater(decoded.shape[0], heatmap.shape[0])
            self.assertEqual(decoded.shape[1], heatmap.shape[1])


if __name__ == "__main__":
    unittest.main()
