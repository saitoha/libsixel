"""Exercise batch generation and source-bound selection reuse end to end."""
import contextlib
import hashlib
import io
import json
import sys
import tempfile
import unittest
from pathlib import Path
import numpy as np
from PIL import Image
sys.path.insert(0, str(Path(__file__).resolve().parents[3] / "tools"))
import palette_gradient as tool


class Workflow(unittest.TestCase):
    def test_saved_selection_reuses_source_and_renders_actual_output(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            source = np.broadcast_to(np.linspace([30, 230, 40], [230, 250, 220], 64).astype(np.uint8), (48, 64, 3)).copy()
            quantized = source.copy()
            quantized[:, :, 0] = (quantized[:, :, 0] // 64) * 64
            quantized[:, :, 1] = 232
            quantized[:, :, 2] = (quantized[:, :, 2] // 64) * 64
            Image.fromarray(source).save(root / "source.png")
            Image.fromarray(quantized).save(root / "quantized.png")
            before = (root / "source.png").read_bytes()
            arguments = [str(root / "source.png"), str(root / "quantized.png")]
            with contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(tool.main(arguments + ["--line", "2,10,60,10", "--slice", "G=245", "--output", str(root / "first")]), 0)
                self.assertEqual(tool.main(arguments + ["--selection", str(root / "first/selection.json"), "--output", str(root / "second")]), 0)
            first = json.loads((root / "first/analysis.json").read_text())
            second = json.loads((root / "second/analysis.json").read_text())
            self.assertEqual(first, second)
            self.assertEqual(first["selection"]["source"]["sha256"], hashlib.sha256(before).hexdigest())
            self.assertEqual(first["outputs"][0]["actual_rgb"], quantized[10, 2:61].tolist())
            self.assertEqual((root / "source.png").read_bytes(), before)
            for path in (root / "first").glob("*.png"):
                with Image.open(path) as image:
                    image.verify()
            self.assertEqual(len(list((root / "first").glob("0*.svg"))), 5)
            self.assertIn("Off-plane distance", (root / "first/report.html").read_text())

    def test_selection_from_another_source_is_rejected(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            image = Image.new("RGB", (16, 16), (30, 220, 50))
            image.save(root / "input.png")
            (root / "selection.json").write_text(json.dumps({"version": 1, "source": {"sha256": "wrong"}, "line": [[1, 1], [8, 8]]}))
            with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as error:
                tool.main([str(root / "input.png"), str(root / "input.png"), "--selection", str(root / "selection.json"), "--output", str(root / "result")])
            self.assertEqual(error.exception.code, 2)

    def test_generated_figure_cannot_overwrite_input(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            image = root / "03-rgb-3d.png"
            Image.new("RGB", (16, 16), (30, 220, 50)).save(image)
            before = image.read_bytes()
            with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as error:
                tool.main([str(image), str(image), "--line", "1,1,8,8", "--output", str(root)])
            self.assertEqual(error.exception.code, 2)
            self.assertEqual(image.read_bytes(), before)


if __name__ == "__main__":
    unittest.main()
