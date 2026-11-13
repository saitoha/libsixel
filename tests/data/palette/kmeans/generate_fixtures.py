#!/usr/bin/env python3
"""Regenerate the native 16x16 gradient and single-pixel rare-color inputs.

This is an offline authoring tool, never invoked by a test. References in
references.json are reviewed regression anchors and are not regenerated here.
"""
import argparse
import colorsys
import importlib.util
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    directory = Path(__file__).resolve().parent
    source = directory.parents[3] / "tools/generate_palette_pipeline_suite_fixtures.py"
    spec = importlib.util.spec_from_file_location("palette_fixtures", source)
    generator = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(generator)
    # An odd stride permutes all 256 locations, without collision retries.
    rare = {
        (i * 73 + 19) % 256: tuple(round(channel * 255) for channel in
            colorsys.hsv_to_rgb((i * .6180339887498949) % 1,
                .72 + .27 * (i % 7) / 6, .70 + .29 * (i % 11) / 10))
        for i in range(32)
    }

    def rare_pixel(x, y, width, height):
        return rare.get(y * width + x, (24 + x * 13 + (y // 4) * 3,) * 3)

    for name, pixel in (("gradient", generator.smooth_gradient),
                        ("rare", rare_pixel)):
        path = directory / (name + "-16.png")
        data = generator.encode_png(16, 16, pixel)
        if args.check:
            if path.read_bytes() != data:
                raise SystemExit("fixture differs: " + str(path))
        else:
            path.write_bytes(data)


if __name__ == "__main__":
    main()
