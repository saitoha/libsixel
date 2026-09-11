#!/usr/bin/env python3
"""Generate small PPM oracles for exact scalar resampling tests.

The calculations in this file intentionally duplicate the documented
coordinate and kernel equations instead of calling libsixel, ImageMagick, or
another image scaler.  The generated expectations therefore remain an
independent oracle for the C implementation.
"""

from __future__ import annotations

import argparse
import json
import math
import random
from pathlib import Path
from typing import Callable, Dict, List, Sequence, Tuple


Kernel = Callable[[float], float]


def bilinear(distance: float) -> float:
    return 1.0 - distance if distance < 1.0 else 0.0


def welsh(distance: float) -> float:
    return 1.0 - distance * distance if distance < 1.0 else 0.0


def bicubic(distance: float) -> float:
    if distance <= 1.0:
        return 1.0 + (distance - 2.0) * distance * distance
    if distance <= 2.0:
        return 4.0 + distance * (
            -8.0 + distance * (5.0 - distance)
        )
    return 0.0


def sinc(value: float) -> float:
    if value == 0.0:
        return 1.0
    return math.sin(math.pi * value) / (math.pi * value)


def lanczos(distance: float, lobes: int) -> float:
    if distance == 0.0:
        return 1.0
    if distance < float(lobes):
        return sinc(distance) * sinc(distance / float(lobes))
    return 0.0


METHODS: Dict[str, Tuple[Kernel | None, float]] = {
    "nearest": (None, 0.0),
    "gaussian": (
        lambda distance: math.exp(-2.0 * distance * distance)
        * math.sqrt(2.0 / math.pi),
        1.0,
    ),
    "hanning": (
        lambda distance: 0.5 + 0.5 * math.cos(math.pi * distance),
        1.0,
    ),
    "hamming": (
        lambda distance: 0.54 + 0.46 * math.cos(math.pi * distance),
        1.0,
    ),
    "bilinear": (bilinear, 1.0),
    "welsh": (welsh, 1.0),
    "bicubic": (bicubic, 2.0),
    "lanczos2": (lambda distance: lanczos(distance, 2), 2.0),
    "lanczos3": (lambda distance: lanczos(distance, 3), 3.0),
    "lanczos4": (lambda distance: lanczos(distance, 4), 4.0),
}


def safe_tones() -> Tuple[int, ...]:
    """Return byte values preserved by SIXEL's percent color definition."""
    return tuple((255 * percent + 50) // 100 for percent in range(101))


def srgb_decode_byte(value: int) -> float:
    """Decode one normalized sRGB byte with the IEC 61966-2-1 curve."""
    encoded = float(value) / 255.0
    if encoded <= 0.04045:
        return encoded / 12.92
    return ((encoded + 0.055) / 1.055) ** 2.4


def srgb_encode_byte(value: float) -> int:
    """Encode one normalized linear-light value to the nearest sRGB byte."""
    if value <= 0.0031308:
        encoded = value * 12.92
    else:
        encoded = 1.055 * value ** (1.0 / 2.4) - 0.055
    return max(0, min(255, round(encoded * 255.0)))


def scale_row(
    source: Sequence[int], destination_width: int, method: str
) -> Tuple[List[int], List[float]]:
    """Apply the documented scalar byte operator to one grayscale row."""
    source_width = len(source)
    kernel, radius = METHODS[method]
    output: List[int] = []
    unrounded: List[float] = []

    if kernel is None:
        for destination_x in range(destination_width):
            source_x = destination_x * source_width // destination_width
            value = source[source_x]
            output.append(value)
            unrounded.append(float(value))
        return output, unrounded

    for destination_x in range(destination_width):
        if destination_width >= source_width:
            center = (
                (float(destination_x) + 0.5)
                * float(source_width)
                / float(destination_width)
            )
            first = max(int(center - radius), 0)
            last = min(int(center + radius), source_width - 1)
        else:
            center = float(destination_x) + 0.5
            first = max(
                math.floor(
                    (center - radius)
                    * float(source_width)
                    / float(destination_width)
                ),
                0,
            )
            last = min(
                math.floor(
                    (center + radius)
                    * float(source_width)
                    / float(destination_width)
                ),
                source_width - 1,
            )

        weighted = 0.0
        total = 0.0
        for source_x in range(first, last + 1):
            if destination_width >= source_width:
                distance = (float(source_x) + 0.5) - center
            else:
                distance = (
                    (float(source_x) + 0.5)
                    * float(destination_width)
                    / float(source_width)
                    - center
                )
            weight = kernel(abs(distance))
            weighted += float(source[source_x]) * weight
            total += weight

        value = weighted / total
        unrounded.append(value)
        output.append(max(0, min(255, math.floor(value))))

    return output, unrounded


def differs_from_nearest(
    source: Sequence[int], destination_width: int, method: str,
    expected: Sequence[int]
) -> bool:
    """Reject filtered fixtures that collapse to nearest-neighbor output."""
    if method == "nearest":
        return True
    nearest, _ = scale_row(source, destination_width, "nearest")
    return list(expected) != nearest


def output_has_margin(method: str, values: Sequence[float]) -> bool:
    """Keep transcendental cases away from unstable integer boundaries."""
    if method == "nearest":
        return True
    for value in values:
        if value <= 0.0 or value >= 255.0:
            continue
        if abs(value - round(value)) < 0.02:
            return False
    return True


def find_fixture(
    method: str, source_width: int, destination_width: int, seed: str
) -> Tuple[List[int], List[int], List[float]]:
    """Find a deterministic nontrivial fixture with safe-tone output."""
    tones = set(safe_tones())
    random_source = random.Random(seed)

    for _ in range(2_000_000):
        source = [random_source.randrange(256) for _ in range(source_width)]
        expected, unrounded = scale_row(source, destination_width, method)
        if any(value not in tones for value in expected):
            continue
        if len(set(expected)) < min(3, destination_width):
            continue
        if not output_has_margin(method, unrounded):
            continue
        if not differs_from_nearest(
            source, destination_width, method, expected
        ):
            continue
        return source, expected, unrounded

    raise RuntimeError(f"unable to find exact fixture for {method}/{seed}")


def write_ppm(path: Path, width: int, height: int,
              grayscale: Sequence[int]) -> None:
    """Write a binary P6 image with replicated grayscale RGB channels."""
    if len(grayscale) != width * height:
        raise ValueError("PPM pixel count does not match dimensions")
    pixels = bytearray()
    for value in grayscale:
        pixels.extend((value, value, value))
    path.write_bytes(f"P6\n{width} {height}\n255\n".encode("ascii") + pixels)


def write_safe_palette(path: Path) -> None:
    """Write every reversible grayscale tone as a JASC fixed palette."""
    tones = safe_tones()
    lines = ["JASC-PAL", "0100", str(len(tones))]
    lines.extend(f"{value} {value} {value}" for value in tones)
    path.write_text("\n".join(lines) + "\n", encoding="ascii")


def generate(output_directory: Path) -> None:
    """Generate method, precision, SIMD, and thread fixtures."""
    manifest: Dict[str, object] = {
        "oracle": "documented scalar equations, independent of libsixel",
        "safe_tones": list(safe_tones()),
        "cases": {},
    }
    cases = manifest["cases"]
    assert isinstance(cases, dict)
    output_directory.mkdir(parents=True, exist_ok=True)
    write_safe_palette(output_directory / "safe-gray.pal")

    for method in METHODS:
        for direction, source_width, destination_width in (
            ("downscale", 16, 4),
            ("upscale", 3, 4),
        ):
            source, expected, unrounded = find_fixture(
                method,
                source_width,
                destination_width,
                f"libsixel-resampling-exact-v1:{method}:{direction}",
            )
            stem = f"{method}-{direction}"
            write_ppm(
                output_directory / f"{stem}-input.ppm",
                source_width,
                1,
                source,
            )
            write_ppm(
                output_directory / f"{stem}-expected.ppm",
                destination_width,
                1,
                expected,
            )
            cases[stem] = {
                "source_dimensions": [source_width, 1],
                "target_dimensions": [destination_width, 1],
                "source": source,
                "expected": expected,
                "unrounded": [f"{value:.12f}" for value in unrounded],
            }

    thread_source, thread_expected, thread_unrounded = find_fixture(
        "bilinear", 16, 4, "libsixel-resampling-exact-v1:thread"
    )
    write_ppm(
        output_directory / "bilinear-thread-input.ppm",
        16,
        16,
        thread_source * 16,
    )
    write_ppm(
        output_directory / "bilinear-thread-expected.ppm",
        4,
        4,
        thread_expected * 4,
    )
    cases["bilinear-thread"] = {
        "source_dimensions": [16, 16],
        "target_dimensions": [4, 4],
        "source_row": thread_source,
        "expected_row": thread_expected,
        "unrounded_row": [f"{value:.12f}" for value in thread_unrounded],
    }

    precision_source = [0, 66]
    preserve_midpoint = (precision_source[0] + precision_source[1]) // 2
    linear_midpoint = srgb_encode_byte(
        (
            srgb_decode_byte(precision_source[0])
            + srgb_decode_byte(precision_source[1])
        )
        / 2.0
    )
    precision_preserve = [precision_source[0], preserve_midpoint,
                          precision_source[1]]
    precision_linear = [precision_source[0], linear_midpoint,
                        precision_source[1]]
    if preserve_midpoint != 33 or linear_midpoint != 46:
        raise RuntimeError("precision oracle no longer has documented values")
    if any(
        value not in safe_tones()
        for value in precision_preserve + precision_linear
    ):
        raise RuntimeError("precision oracle is not SIXEL safe-tone exact")
    write_ppm(
        output_directory / "bilinear-precision-input.ppm",
        2,
        1,
        precision_source,
    )
    write_ppm(
        output_directory / "bilinear-precision-preserve-expected.ppm",
        3,
        1,
        precision_preserve,
    )
    write_ppm(
        output_directory / "bilinear-precision-linear-expected.ppm",
        3,
        1,
        precision_linear,
    )
    cases["bilinear-precision"] = {
        "source_dimensions": [2, 1],
        "target_dimensions": [3, 1],
        "source": precision_source,
        "preserve_expected": precision_preserve,
        "linear_and_float_expected": precision_linear,
        "calculation": {
            "preserve_midpoint": "floor((0 + 66) / 2) = 33",
            "linear_midpoint": (
                "round(255 * srgb_encode((srgb_decode(0) + "
                "srgb_decode(66)) / 2)) = 46"
            ),
        },
    }

    (output_directory / "fixtures.json").write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--output-directory",
        type=Path,
        default=Path("tests/data/inputs/resampling-exact"),
    )
    args = parser.parse_args()
    generate(args.output_directory)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
