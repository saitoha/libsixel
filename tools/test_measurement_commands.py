"""Regression tests for validating archived and newly measured argv."""

import csv
import json
import shlex
import tempfile
import unittest
from pathlib import Path

from check_working_colorspace_measurements import validate_command
from measurement_commands import canonical_encoder_arguments
from plot_lookup_policy_delaunay import load_validated


ROOT = Path(__file__).resolve().parents[1]


class MeasurementCommandsTest(unittest.TestCase):
    def test_encoder_options_normalize_without_touching_other_arguments(self):
        recorded = ["-Xcielab", "-W", "linear",
                    "--output-colorspace=adobe-rgb:gamut=clip",
                    "-Lbuiltin:cms_target=gamma!", "--precision=float32",
                    "--", "-Wgamma"]
        expected = ["-Xlab-d65", "-Wsrgb-linear",
                    "-Ua98-rgb:gamut=clip", "-Lbuiltin:cms_target=gamma!",
                    "--precision-policy=float32", "--", "-Wgamma"]
        before = recorded.copy()
        self.assertEqual(canonical_encoder_arguments(recorded), expected)
        self.assertEqual(recorded, before)

    def test_working_space_accepts_recorded_and_new_commands(self):
        path = (ROOT / "docs/encoder/palette-application/working-colorspace"
                / "measurements/working-colorspace-comparison.csv")
        with path.open(newline="") as stream:
            row = next(csv.DictReader(stream))
        validate_command(row, path)
        current = {"-Xgamma": "-Xsrgb", "-Wgamma": "-Wsrgb",
                   "-Ugamma": "-Usrgb", "--precision=float32":
                   "--precision-policy=float32"}
        arguments = [current.get(arg, arg)
                     for arg in shlex.split(row["command"])]
        new_row = dict(row, command=shlex.join(arguments))
        validate_command(new_row, path)
        with self.assertRaisesRegex(ValueError, "command controls missing"):
            validate_command(dict(new_row, command=new_row["command"].replace(
                "-Wsrgb", "-Wsrgb-linear")), path)

    def test_delaunay_accepts_recorded_and_new_protocols(self):
        directory = (ROOT / "docs/encoder/palette-application/lookup-policy"
                     / "measurements")
        data = load_validated(directory)
        for row in data["results"]:
            row["command"] = row["command"].replace("-Wgamma", "-Wsrgb").replace(
                "--precision=8bit", "--precision-policy=auto")
        with tempfile.TemporaryDirectory() as scratch:
            directory = Path(scratch)
            path = directory / "lookup-policy-delaunay-run.json"
            path.write_text(json.dumps(data))
            self.assertEqual(load_validated(directory), data)
            first = data["results"][0]
            first["command"] = first["command"].replace("-Wsrgb", "-Wsrgb-linear")
            path.write_text(json.dumps(data))
            with self.assertRaisesRegex(ValueError, "comparison protocol"):
                load_validated(directory)


if __name__ == "__main__":
    unittest.main()
