"""Regression tests for validating archived and newly measured argv."""

import csv
import json
import shlex
import tempfile
import unittest
from pathlib import Path

import check_dither_policy_measurements as dither_policy
import check_palette_pipeline_measurements as palette_pipeline
import check_precision_measurements as precision
import check_quantize_model_measurements as quantize_model
import check_threading_measurements as threading
from check_working_colorspace_measurements import validate_command
from measurement_commands import (
    canonical_encoder_arguments,
    recorded_colorspace_fields,
)
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
            recorded = next(csv.DictReader(stream))
        row = recorded_colorspace_fields(recorded)
        validate_command(row, path)
        current = {"-Xgamma": "-Xsrgb", "-Wgamma": "-Wsrgb",
                   "-Ugamma": "-Usrgb", "--precision=float32":
                   "--precision-policy=float32"}
        arguments = [current.get(arg, arg)
                     for arg in shlex.split(row["command"])]
        new_row = dict(row, command=shlex.join(arguments))
        validate_command(new_row, path)
        self.assertEqual(new_row["colorspace"], "srgb")
        self.assertEqual(recorded["colorspace"], "gamma")
        with self.assertRaisesRegex(ValueError, "command controls missing"):
            validate_command(dict(new_row, command=new_row["command"].replace(
                "-Wsrgb", "-Wsrgb-linear")), path)

    def test_measurement_fields_leave_unrelated_values_untouched(self):
        recorded = {"protocol": {"colorspaces": [
            {"name": "linear", "label": "linear"}],
            "resize_precision": "linear", "working_colorspace": "gamma"},
            "rows": [{"domain": "clustering-colorspace", "config": "cielab",
                      "option": "-Xcielab"}]}
        current = recorded_colorspace_fields(recorded)
        self.assertEqual(current["protocol"]["colorspaces"],
                         [{"name": "srgb-linear", "label": "linear"}])
        self.assertEqual(current["protocol"]["resize_precision"], "linear")
        self.assertEqual(current["protocol"]["working_colorspace"], "srgb")
        self.assertEqual(current["rows"][0]["config"], "lab-d65")
        self.assertEqual(current["rows"][0]["option"], "-Xlab-d65")
        self.assertEqual(recorded["rows"][0]["option"], "-Xcielab")
        self.assertEqual(recorded_colorspace_fields(current), current)

    def test_precision_controls_match_recorded_and_current_commands(self):
        cases = (
            ("encoder/palette-application/dithering", "dither-policy",
             dither_policy.validate_command),
            ("encoder/palette-construction/pipeline", "palette-pipeline",
             lambda row, path, value: palette_pipeline.validate_command(
                 row, path, precision=value)),
            ("encoder/palette-construction/quantization", "quantize-model",
             lambda row, path, value: quantize_model.validate_command(
                 row, path, precision=value)),
        )
        for document, name, check in cases:
            with self.subTest(measurement=name):
                path = ROOT / "docs" / document / "measurements" / f"{name}-quality.csv"
                with path.open(newline="") as stream:
                    row = next(csv.DictReader(stream))
                check(row, path, "8bit")
                args = canonical_encoder_arguments(shlex.split(row["command"]))
                args[args.index("--precision-policy=8bit")] = "--precision-policy=auto"
                legacy_quantize = "--quantize-model=kmeans:merge=ward:seed=1"
                if legacy_quantize in args:
                    index = args.index(legacy_quantize)
                    args[index:index + 1] = ["--quantize-model=kmeans:seed=1",
                                             "--merge-policy=ward"]
                current = dict(row, command=shlex.join(args))
                check(current, path, "auto")
                with self.assertRaisesRegex(ValueError, "precision"):
                    check(current, path, "8bit")

    def test_precision_manifest_validates_both_recorded_configurations(self):
        path = (ROOT / "docs/encoder/precision/measurements/precision-run.json")
        records = precision.current_measurement_records(ROOT / "tools")
        recorded = precision.load_metadata(path, records)
        self.assertEqual(len(records), len(recorded["protocol"]["domains"]) + 1)
        recorded["source"]["revision"] = "current"
        recorded["protocol"]["domains"] = records
        with tempfile.TemporaryDirectory() as scratch:
            current = Path(scratch) / path.name
            current.write_text(json.dumps(recorded))
            self.assertEqual(precision.load_metadata(current, records), recorded)
            recorded["protocol"]["domains"] = records[:-1]
            current.write_text(json.dumps(recorded))
            with self.assertRaisesRegex(ValueError, "configuration manifest"):
                precision.load_metadata(current, records)

    def test_threading_budget_requires_the_recorded_precision(self):
        directory = ROOT / "docs/threading/measurements"
        metadata = json.loads((directory / "threading-run.json").read_text())
        commands = metadata["protocol"]["encoder_commands"]
        revision = metadata["source"]["revision"]
        path = directory / "thread-budget.csv"
        threading.validate_budget(path, revision, commands, "8bit")

        def current_command(command):
            args = canonical_encoder_arguments(shlex.split(command))
            args[args.index("--precision-policy=8bit")] = "--precision-policy=auto"
            return shlex.join(args)

        with path.open(newline="") as stream:
            reader = csv.DictReader(stream)
            rows = list(reader)
            fields = reader.fieldnames
        for row in rows:
            row["command"] = current_command(row["command"])
        with tempfile.TemporaryDirectory() as scratch:
            current = Path(scratch) / path.name
            with current.open("w", newline="") as stream:
                writer = csv.DictWriter(stream, fieldnames=fields)
                writer.writeheader()
                writer.writerows(rows)
            threading.validate_budget(
                current, revision, [current_command(command) for command in commands],
                "auto")
            with self.assertRaisesRegex(ValueError, "budget policy"):
                threading.validate_budget(current, revision, commands, "8bit")

    def test_delaunay_accepts_recorded_and_new_protocols(self):
        directory = (ROOT / "docs/encoder/palette-application/lookup-policy"
                     / "measurements")
        data = load_validated(directory)
        data["source"]["revision"] = "1" * 40
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
