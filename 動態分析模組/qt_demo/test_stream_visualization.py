from __future__ import annotations

import unittest

import numpy as np

from stream_visualization import (
    detector_image,
    detector_image_with_range,
    false_color_lines,
    resolve_band_indices,
)


class StreamVisualizationTests(unittest.TestCase):
    def test_resolve_band_indices_uses_nearest_wavelengths(self) -> None:
        wavelengths = np.array([900.0, 1000.0, 1300.0, 1650.0, 1700.0])
        indices, actual = resolve_band_indices(
            wavelengths,
            band_count=5,
            targets_nm=(1650.0, 1300.0, 1000.0),
        )
        self.assertEqual(indices, (3, 2, 1))
        self.assertEqual(actual, (1650.0, 1300.0, 1000.0))

    def test_false_color_lines_maps_channels(self) -> None:
        corrected = np.zeros((2, 4, 3), dtype=np.float32)
        corrected[:, 3, :] = 0.9
        corrected[:, 2, :] = 0.45
        corrected[:, 1, :] = 0.0
        image = false_color_lines(corrected, (3, 2, 1))
        self.assertEqual(image.shape, (2, 3, 3))
        np.testing.assert_array_equal(image[0, 0], [255, 128, 0])

    def test_detector_image_flips_spectral_axis(self) -> None:
        raw = np.array([[0.0, 0.0], [10.0, 10.0], [20.0, 20.0]])
        image = detector_image(
            raw,
            percentile_low=0.0,
            percentile_high=100.0,
        )
        self.assertEqual(image.shape, (3, 2, 3))
        self.assertTrue(np.all(image[0] == 255))
        self.assertTrue(np.all(image[-1] == 0))

    def test_detector_image_reports_displayed_dn_range(self) -> None:
        raw = np.arange(12, dtype=np.float32).reshape(3, 4)
        image, value_range = detector_image_with_range(
            raw,
            percentile_low=0.0,
            percentile_high=100.0,
        )

        self.assertEqual(image.shape, (3, 4, 3))
        self.assertEqual(value_range, (0.0, 11.0))


if __name__ == "__main__":
    unittest.main()
