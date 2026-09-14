"""Keep automatic extraction source-only, deterministic and edge-sensitive."""
import sys
import unittest
from pathlib import Path
import numpy as np
sys.path.insert(0, str(Path(__file__).resolve().parents[3] / "tools"))
import palette_gradient as tool


def opaque(rgb):
    return np.concatenate([rgb, np.full((*rgb.shape[:2], 1), 255, dtype=np.uint8)], axis=2)


class Extraction(unittest.TestCase):
    def test_directional_ramp_and_roi_use_original_coordinates(self):
        rgb = np.broadcast_to(np.linspace([20, 230, 30], [240, 255, 245], 128).astype(np.uint8), (96, 128, 3)).copy()
        source = opaque(rgb)
        found = tool.discover_gradients(source, 4, [20, 20, 90, 50])
        self.assertTrue(found)
        self.assertEqual(found, tool.discover_gradients(source, 4, [20, 20, 90, 50]))
        for row in found:
            line = np.array(row["line"])
            self.assertTrue((line >= [20, 20]).all())
            self.assertTrue((line < [110, 70]).all())
            self.assertLess(abs(line[1, 1] - line[0, 1]), 2)

    def test_flat_edge_and_texture_are_not_smooth_ramps(self):
        flat = np.full((96, 128, 3), 150, dtype=np.uint8)
        edge = np.zeros_like(flat)
        edge[:, 64:] = 255
        noise = np.random.default_rng(13).integers(0, 256, flat.shape, dtype=np.uint8)
        for image in [flat, edge, noise]:
            with self.subTest(image=image[0, 0].tolist()):
                self.assertEqual(tool.discover_gradients(opaque(image), 4), [])

    def test_line_sampling_preserves_pixels_and_rejects_alpha(self):
        data = opaque(np.arange(60, dtype=np.uint8).reshape(4, 5, 3))
        xy, pixels = tool.line_pixels(data, [[2, 0], [2, 3]])
        np.testing.assert_array_equal(pixels, data[:, 2, :3])
        np.testing.assert_array_equal(xy[:, 0], 2)
        data[1, 2, 3] = 0
        with self.assertRaisesRegex(ValueError, "transparent"):
            tool.line_pixels(data, [[2, 0], [2, 3]])
        with self.assertRaisesRegex(ValueError, "outside"):
            tool.line_pixels(data, [[-1, 0], [2, 3]])


if __name__ == "__main__":
    unittest.main()
