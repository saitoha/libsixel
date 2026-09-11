#!/usr/bin/env python3
"""Generate reproducible sRGB illustrations of source-color misinterpretation.

Requires NumPy and Pillow >= 10.1 (bundled Aileron font). This numerical
illustration does not invoke or benchmark a libsixel CMS backend.
"""

from __future__ import annotations

import argparse
import hashlib
import io
import json
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont, PngImagePlugin

ROOT = Path(__file__).resolve().parent.parent
OUTPUT = ROOT / "docs/loader/cms-examples"
SOURCE = ROOT / "images/snake.png"
# D65 primary chromaticities from CSS Color 4. Matrices are derived below,
# rather than using rounded inverse matrices which could bias the comparison.
SRGB = ((0.64, 0.33), (0.30, 0.60), (0.15, 0.06))
P3 = ((0.68, 0.32), (0.265, 0.69), (0.15, 0.06))
WHITE = (0.3127, 0.3290)
INK = "#182B3A"
MUTED = "#526574"
PAPER = "#FFFFFF"
ACCENT = "#136A77"


def decode_srgb(values):
    """Decode bounded gamma-encoded sRGB samples to linear light."""
    return np.where(values <= 0.04045, values / 12.92,
                    ((values + 0.055) / 1.055) ** 2.4)


def encode_srgb(values):
    """Encode linear light; clip only insignificant round-trip roundoff."""
    assert values.min() >= -1e-12 and values.max() <= 1.0 + 1e-12
    values = np.clip(values, 0.0, 1.0)
    return np.where(values <= 0.0031308, values * 12.92,
                    1.055 * values ** (1.0 / 2.4) - 0.055)


def matrix(primaries):
    """Construct RGB-to-XYZ with unit-luminance D65 white."""
    columns = np.array([(x / y, 1.0, (1.0 - x - y) / y)
                        for x, y in primaries]).T
    x, y = WHITE
    white = np.array([x / y, 1.0, (1.0 - x - y) / y])
    return columns * np.linalg.solve(columns, white)


def quantize(values):
    """Round display values to nearest byte only at the preview boundary."""
    return np.floor(np.clip(values, 0, 1) * 255 + 0.5).astype(np.uint8)


def variants(reference):
    """Re-encode identical colors and verify their correct reconstruction."""
    linear = decode_srgb(reference)
    transform = np.linalg.solve(matrix(P3), matrix(SRGB))
    p3_linear = linear @ transform.T
    p3_source = encode_srgb(p3_linear)
    restored = encode_srgb(decode_srgb(p3_source) @ np.linalg.inv(transform).T)
    gamma_source = linear ** (1.0 / 1.8)
    gamma_restored = encode_srgb(gamma_source ** 1.8)
    linear_restored = encode_srgb(linear)
    # Starting in sRGB keeps every example in both gamuts. No gamut mapping
    # or source quantization is needed to recover the reference appearance.
    assert np.max(np.abs(restored - reference)) < 1e-12
    assert np.max(np.abs(gamma_restored - reference)) < 1e-12
    assert np.max(np.abs(linear_restored - reference)) < 1e-12
    return {"correct": restored, "p3-ignored": p3_source,
            "gamma18-ignored": gamma_source, "linear-ignored": linear}


def png_bytes(image):
    """All panels, including wrong interpretations, are displayed as sRGB."""
    info = PngImagePlugin.PngInfo()
    info.add(b"sRGB", b"\x00")
    output = io.BytesIO()
    image.save(output, format="PNG", pnginfo=info, compress_level=9)
    return output.getvalue()


def label(draw, xy, value, size=24, fill=INK):
    """Use Pillow's bundled font, avoiding host-specific font selection."""
    font = ImageFont.load_default(size=size)
    # Fail instead of silently shrinking or clipping labels on a new layout.
    assert draw.textbbox(xy, value, font=font)[2] <= draw.im.size[0] - 18
    draw.text(xy, value, fill=fill, font=font)


def panel(canvas, x, y, photo, chips, title, subtitle, note):
    """Draw one same-sized photo, six chips, and an explanatory label."""
    draw = ImageDraw.Draw(canvas)
    label(draw, (x, y), title, 27)
    label(draw, (x, y + 38), subtitle, 21, MUTED)
    canvas.paste(Image.fromarray(quantize(photo)), (x, y + 78))
    for index, color in enumerate(quantize(chips)):
        left = x + index * 100
        draw.rectangle((left, y + 550, left + 99, y + 615),
                       fill=tuple(int(c) for c in color))
    label(draw, (x, y + 633), note, 20, MUTED)


def figure(photos, chips, kind, mobile):
    """Keep identical panel geometry while stacking for narrow displays."""
    if kind == "gamut":
        title = "When the source color gamut is ignored"
        subtitle = "Same colors, encoded as Display P3. Only interpretation changes."
        panels = [
            ("correct", "Correctly converted to sRGB", "Source primaries are respected",
             "Reference: red, orange, green, cyan, blue, gray"),
            ("p3-ignored", "P3 numbers treated as sRGB", "The source profile is ignored",
             "Color balance changes; neutral gray stays neutral"),
        ]
    else:
        title = "When the source transfer function is ignored"
        subtitle = "Same colors and sRGB primaries. Only the transfer encoding changes."
        panels = [
            ("correct", "Correctly converted to sRGB", "Gamma 1.8 or linear source, interpreted correctly",
             "Reference gray steps: 32, 64, 96, 128, 192, 224"),
            ("gamma18-ignored", "Gamma 1.8 treated as sRGB", "A different curve darkens the midtones",
             "The reference gray 128 becomes 109"),
            ("linear-ignored", "Linear light treated as sRGB", "A stronger mismatch: the transfer step is missing",
             "The reference gray 128 becomes 55"),
        ]
    columns = 1 if mobile else len(panels)
    width = columns * 600 + (columns + 1) * 24
    height = 164 + (len(panels) if mobile else 1) * 690 + 58
    canvas = Image.new("RGB", (width, height), PAPER)
    draw = ImageDraw.Draw(canvas)
    label(draw, (24, 22), "LOADER CMS / CONTROLLED COLOR EXAMPLE", 18, ACCENT)
    if mobile:
        heading = ("Source gamut ignored" if kind == "gamut"
                   else "Source transfer function ignored")
        label(draw, (24, 58), heading, 30)
        label(draw, (24, 104), "Same intended image; different interpretation.", 22, MUTED)
    else:
        label(draw, (24, 58), title, 34)
        label(draw, (24, 109), subtitle, 23, MUTED)
    for index, (key, heading, detail, note) in enumerate(panels):
        x = 24 if mobile else 24 + index * 624
        y = 164 + index * 690 if mobile else 164
        panel(canvas, x, y, photos[key], chips[key], heading, detail, note)
    label(draw, (24, height - 46),
          "All panels are sRGB previews. No saturation or contrast boost.", 20, MUTED)
    return png_bytes(canvas)


def generate():
    """Return every tracked asset as bytes, including numerical evidence."""
    source = np.asarray(Image.open(SOURCE).convert("RGB"), dtype=np.float64) / 255
    assert source.shape == (450, 600, 3)
    # Assign the fixture samples to the illustrative sRGB baseline explicitly;
    # the fixture's approximate gAMA chunk is not a source profile in this model.
    colors = np.array([[230, 50, 50], [240, 140, 35], [40, 180, 70],
                       [30, 180, 200], [60, 90, 220], [128, 128, 128]]) / 255
    grays = np.repeat(np.array([32, 64, 96, 128, 192, 224])[:, None], 3, axis=1) / 255
    photos = variants(source)
    gamut_chips = variants(colors)
    gamma_chips = variants(grays)
    # Check semantic anchors as well as the algebraic inverse: neutral P3
    # samples remain neutral, correct previews retain every source byte,
    # and the published gamma example has the stated scalar values.
    assert np.array_equal(quantize(photos["correct"]), quantize(source))
    assert np.allclose(gamma_chips["p3-ignored"], grays, atol=1e-12, rtol=0)
    assert quantize(gamma_chips["gamma18-ignored"])[3, 0] == 109
    assert quantize(gamma_chips["linear-ignored"])[3, 0] == 55
    assets = {}
    for kind, chips in [("gamut", gamut_chips), ("gamma", gamma_chips)]:
        for mobile in [False, True]:
            suffix = "mobile" if mobile else "wide"
            assets[f"{kind}-{suffix}.png"] = figure(photos, chips, kind, mobile)
    evidence = {
        "generator": "tools/plot_loader_cms_examples.py",
        "source": "images/snake.png",
        "source_sha256": hashlib.sha256(SOURCE.read_bytes()).hexdigest(),
        "method": "Analytical re-encoding, not a libsixel backend measurement",
        "baseline": "Fixture RGB bytes explicitly interpreted as sRGB",
        "source_quantization": "None; float64 until sRGB preview rounding",
        "gamut_mapping": "None required: reference colors are inside sRGB and P3",
        "reference": "https://www.w3.org/TR/css-color-4/#predefined-display-p3",
        "srgb_primaries": SRGB, "p3_primaries": P3, "white_xy": WHITE,
        "gamma_exponents": [1.8, 1.0],
        "gamut_chips_srgb_bytes": {k: quantize(v).tolist() for k, v in gamut_chips.items()
                                   if k in ["correct", "p3-ignored"]},
        "gray_steps_srgb_bytes": {k: quantize(v)[:, 0].tolist() for k, v in gamma_chips.items()
                                  if k != "p3-ignored"},
        "roundtrip_max_abs_error_bound": 1e-12,
        "preview_sha256": {k: hashlib.sha256(v).hexdigest() for k, v in assets.items()},
    }
    assets["examples.json"] = (json.dumps(evidence, indent=2) + "\n").encode()
    return assets


def main():
    """Check byte-for-byte reproducibility without modifying tracked assets."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    assets = generate()
    if args.check:
        stale = [name for name, value in assets.items()
                 if not (OUTPUT / name).exists() or (OUTPUT / name).read_bytes() != value]
        if stale:
            raise SystemExit("Stale CMS examples: " + ", ".join(stale))
        print("CMS examples: round trips and generated assets PASS")
    else:
        OUTPUT.mkdir(parents=True, exist_ok=True)
        for name, value in assets.items():
            (OUTPUT / name).write_bytes(value)
        print("Generated " + str(OUTPUT.relative_to(ROOT)))


if __name__ == "__main__":
    main()
