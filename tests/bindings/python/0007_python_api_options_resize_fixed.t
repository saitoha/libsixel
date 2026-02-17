#!/bin/sh
# Exercise explicit width/height resize options.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${ENABLE_PYTHON:-0}" = "1" || \
    skip_all "python bindings are disabled in this build"

test -n "${SIXEL_TEST_PYTHON_VENV:-}" || \
    skip_all "python wheel test environment is unavailable"

test -x "${SIXEL_TEST_PYTHON_VENV}/bin/python" || \
    skip_all "python wheel test environment is unavailable"

run_python="${SIXEL_TEST_PYTHON_VENV}/bin/python"
libdir="${LIBSIXEL_LIBDIR:-${TOP_BUILDDIR}/src/.libs}"
test -d "${libdir}" || libdir="${TOP_BUILDDIR}/src"

python_output=$(env \
    LIBSIXEL_LIBDIR="${libdir}" \
    LD_LIBRARY_PATH="${libdir}${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}" \
    DYLD_LIBRARY_PATH="${libdir}${DYLD_LIBRARY_PATH:+:${DYLD_LIBRARY_PATH}}" \
    "${run_python}" - \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" \
    "${ARTIFACT_LOCAL_DIR}/resize_fixed" 2>&1 <<'PY'
import pathlib
import re
import sys
from typing import Iterable, Tuple

try:
    from libsixel_wheel import (
        SIXEL_OPTFLAG_HEIGHT,
        SIXEL_OPTFLAG_INPUT,
        SIXEL_OPTFLAG_OUTPUT,
        SIXEL_OPTFLAG_QUALITY,
        SIXEL_OPTFLAG_RESAMPLING,
        SIXEL_OPTFLAG_WIDTH,
    )
    from libsixel_wheel.decoder import Decoder
    from libsixel_wheel.encoder import Encoder
except OSError as exc:
    print(f"SKIP_LIBSIXEL_LOAD:{exc}")
    raise SystemExit(2)


def ensure_sixel_signature(data: bytes) -> None:
    if not data.startswith(b"\x1bPq"):
        raise SystemExit("missing sixel DCS introducer")
    if not data.rstrip(b"\r\n").endswith(b"\x1b\\"):
        raise SystemExit("missing sixel ST terminator")


def extract_raster_size(data: bytes) -> Tuple[int, int] | None:
    match = re.search(rb'"(\d+);(\d+);(\d+);(\d+)', data)
    if match:
        return int(match.group(1)), int(match.group(2))
    return None


def read_png_dimensions(path: pathlib.Path) -> Tuple[int, int]:
    header = path.read_bytes()
    if len(header) < 24 or header[:8] != b"\x89PNG\r\n\x1a\n":
        raise SystemExit("output is not a PNG")
    return int.from_bytes(header[16:20], "big"), int.from_bytes(header[20:24], "big")


def encode_with_options(source: pathlib.Path, target: pathlib.Path,
                        options: Iterable[tuple[int, str]]):
    encoder = Encoder()
    encoder.setopt(SIXEL_OPTFLAG_INPUT, str(source))
    encoder.setopt(SIXEL_OPTFLAG_OUTPUT, str(target))
    for flag, value in options:
        encoder.setopt(flag, value)
    encoder.encode(str(source))
    if not target.exists() or target.stat().st_size == 0:
        raise SystemExit("missing or empty sixel output")
    data = target.read_bytes()
    ensure_sixel_signature(data)
    return extract_raster_size(data)


def decode_to_png(source: pathlib.Path, target: pathlib.Path):
    decoder = Decoder()
    decoder.setopt(SIXEL_OPTFLAG_INPUT, str(source))
    decoder.setopt(SIXEL_OPTFLAG_OUTPUT, str(target))
    decoder.decode(str(source))
    if not target.exists() or target.stat().st_size == 0:
        raise SystemExit("decoder did not write output")
    return read_png_dimensions(target)


source = pathlib.Path(sys.argv[1])
workdir = pathlib.Path(sys.argv[2])
workdir.mkdir(parents=True, exist_ok=True)
output = workdir / "resize_fixed.six"
png = workdir / "resize_fixed.png"
raster = encode_with_options(source, output, [
    (SIXEL_OPTFLAG_WIDTH, "64"),
    (SIXEL_OPTFLAG_HEIGHT, "32"),
    (SIXEL_OPTFLAG_RESAMPLING, "bilinear"),
    (SIXEL_OPTFLAG_QUALITY, "full"),
])
width, height = decode_to_png(output, png)
if (width, height) != (64, 32):
    raise SystemExit(f"expected 64x32, got {width}x{height}")
if raster and (raster[0] > 1 or raster[1] > 1):
    if (raster[0], raster[1]) != (64, 32):
        raise SystemExit("raster attribute mismatch")
print("resize to 64x32 preserved in decode and raster")
PY
)
python_status=$?
printf '%s' "${python_output}" >&2
test "${python_status}" -eq 0 && {
    tap_pass 1 "explicit resize dimensions survive decode via wheel"
    tap_plan 1
    exit 0
}

marker=$(printf '%s' "${python_output}" | awk '/^SKIP_LIBSIXEL_LOAD:/{print; exit}')
test -n "${marker}" && tap_skip_all "libsixel failed to load: ${marker#SKIP_LIBSIXEL_LOAD:}"

tap_fail 1 "explicit resize dimensions via wheel failed"
tap_plan 1
exit 0
