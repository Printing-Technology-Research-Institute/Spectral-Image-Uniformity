from __future__ import annotations

import unittest

import numpy as np

from geometry_calibration import (
    estimate_geometry_calibration,
)


class GeometryCalibrationTests(unittest.TestCase):
    @staticmethod
    def synthetic_circle_cube(
        *,
        lines: int = 360,
        width: int = 200,
        ellipse_width: float = 70.0,
        ellipse_height: float = 210.0,
    ) -> np.ndarray:
        y, x = np.ogrid[:lines, :width]
        center_x = width / 2.0
        center_y = lines / 2.0
        mask = (
            ((x - center_x) / (ellipse_width / 2.0)) ** 2
            + ((y - center_y) / (ellipse_height / 2.0)) ** 2
        ) <= 1.0
        rng = np.random.default_rng(17)
        projection = np.full((lines, width), 0.82, dtype=np.float32)
        projection[mask] = 0.14
        projection += rng.normal(0.0, 0.005, projection.shape).astype(
            np.float32
        )
        return np.repeat(projection[:, :, None], 12, axis=2)

    def test_estimates_line_rate_multiplier_from_stretched_circle(self) -> None:
        result = estimate_geometry_calibration(self.synthetic_circle_cube())

        self.assertAlmostEqual(result.width_px, 70.0, delta=2.5)
        self.assertAlmostEqual(result.height_lines, 210.0, delta=3.0)
        self.assertAlmostEqual(result.height_width_ratio, 3.0, delta=0.12)
        self.assertAlmostEqual(
            result.line_rate_multiplier,
            1.0 / 3.0,
            delta=0.02,
        )
        self.assertGreater(result.solidity, 0.98)

    def test_rejects_non_image_input(self) -> None:
        with self.assertRaisesRegex(ValueError, "lines, width, bands"):
            estimate_geometry_calibration(np.zeros((20, 20), dtype=np.float32))


if __name__ == "__main__":
    unittest.main()
