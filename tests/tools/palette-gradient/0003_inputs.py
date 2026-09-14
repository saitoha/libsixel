"""Preserve indexed palette metadata and reject misleading output inputs."""
import sys
import tempfile
import unittest
from pathlib import Path
import numpy as np
from PIL import Image
sys.path.insert(0, str(Path(__file__).resolve().parents[3] / "tools"))
import palette_gradient as tool


class Inputs(unittest.TestCase):
    def test_unused_indexed_site_is_kept_but_transparent_site_is_not(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "indexed.png"
            image = Image.new("P", (8, 8), 0)
            image.putpalette([100, 250, 100, 150, 250, 150, 200, 250, 200])
            image.save(path, transparency=2)
            rgba, table, record = tool.image_data(path)
            palette = tool.output_palette(rgba, table)
            self.assertEqual(palette["ids"], [0, 1])
            np.testing.assert_array_equal(palette["colors"], [[100, 250, 100], [150, 250, 150]])
            self.assertEqual(record["mode"], "P")

    def test_rgb_output_has_observed_colors_only(self):
        rgba = np.array([[[1, 2, 3, 255], [2, 3, 4, 255], [250, 250, 250, 0]]], dtype=np.uint8)
        palette = tool.output_palette(rgba, None)
        self.assertEqual(len(palette["colors"]), 2)
        self.assertIn("unused palette entries are unavailable", palette["origin"])

    def test_lossy_or_nonquantized_output_is_rejected(self):
        rgba = np.zeros((1, 300, 4), dtype=np.uint8)
        rgba[:, :, 0] = np.arange(300) % 256
        rgba[:, :, 1] = np.arange(300) // 256
        rgba[:, :, 3] = 255
        with self.assertRaisesRegex(ValueError, "300 opaque colors"):
            tool.output_palette(rgba, None)


if __name__ == "__main__":
    unittest.main()
