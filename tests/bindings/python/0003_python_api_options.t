#!/bin/sh
# Exercise encoder options for palette, diffusion, quality, and resizing.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

if [ "${ENABLE_PYTHON:-0}" != "1" ]; then
    skip_all "python bindings are disabled in this build"
fi

if [ -z "${SIXEL_TEST_PYTHON_VENV:-}" ] \
   || [ ! -x "${SIXEL_TEST_PYTHON_VENV}/bin/python" ]; then
    skip_all "python wheel test environment is unavailable"
fi

run_python="${SIXEL_TEST_PYTHON_VENV}/bin/python"
libdir="${LIBSIXEL_LIBDIR:-${TOP_BUILDDIR}/src/.libs}"
if [ ! -d "${libdir}" ]; then
    libdir="${TOP_BUILDDIR}/src"
fi

python_skip_on_load_error() {
    status=$1
    log_text=$2

    if [ "${status}" -eq 0 ]; then
        return 0
    fi

    marker=$(printf '%s' "${log_text}" | grep -m1 '^SKIP_LIBSIXEL_LOAD:' || true)
    if [ -n "${marker}" ]; then
        tap_skip_all "libsixel failed to load: ${marker#SKIP_LIBSIXEL_LOAD:}"
    fi
}

cases=3
case_id=1

run_case() {
    scenario=$1
    description=$2
    working_dir="${ARTIFACT_LOCAL_DIR}/${scenario}"

    python_output=$(env \
        LIBSIXEL_LIBDIR="${libdir}" \
        LD_LIBRARY_PATH="${libdir}${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}" \
        DYLD_LIBRARY_PATH="${libdir}${DYLD_LIBRARY_PATH:+:${DYLD_LIBRARY_PATH}}" \
        "${run_python}" - "${scenario}" \
        "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" \
        "${working_dir}" 2>&1 <<'PY'
import math
import pathlib
import re
import sys
from typing import Iterable, Set, Tuple

try:
    from libsixel_wheel import (
        SIXEL_OPTFLAG_BGCOLOR,
        SIXEL_OPTFLAG_COLORS,
        SIXEL_OPTFLAG_DIFFUSION,
        SIXEL_OPTFLAG_HEIGHT,
        SIXEL_OPTFLAG_INPUT,
        SIXEL_OPTFLAG_OUTPUT,
        SIXEL_OPTFLAG_PALETTE_TYPE,
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


def extract_palette_indexes(data: bytes) -> Set[int]:
    return {int(entry) for entry in re.findall(rb"#(\d+)", data)}


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
    return data, extract_palette_indexes(data), extract_raster_size(data)


def decode_to_png(source: pathlib.Path, target: pathlib.Path):
    decoder = Decoder()
    decoder.setopt(SIXEL_OPTFLAG_INPUT, str(source))
    decoder.setopt(SIXEL_OPTFLAG_OUTPUT, str(target))
    decoder.decode(str(source))
    if not target.exists() or target.stat().st_size == 0:
        raise SystemExit("decoder did not write output")
    return read_png_dimensions(target)


def run_palette_combo(source: pathlib.Path, workdir: pathlib.Path) -> str:
    output = workdir / "palette.six"
    _, palette, raster = encode_with_options(source, output, [
        (SIXEL_OPTFLAG_COLORS, "16"),
        (SIXEL_OPTFLAG_DIFFUSION, "atkinson"),
        (SIXEL_OPTFLAG_PALETTE_TYPE, "hls"),
        (SIXEL_OPTFLAG_QUALITY, "high"),
        (SIXEL_OPTFLAG_BGCOLOR, "#000000"),
    ])
    if not palette or len(palette) > 16:
        raise SystemExit("palette limit check failed")
    if raster:
        return f"palette ok (<=16 entries), raster={raster[0]}x{raster[1]}"
    return "palette ok (<=16 entries)"


def run_fixed_resize(source: pathlib.Path, workdir: pathlib.Path) -> str:
    output = workdir / "resize_fixed.six"
    png = workdir / "resize_fixed.png"
    _, _, raster = encode_with_options(source, output, [
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
    return "resize to 64x32 preserved in decode and raster"


def run_aspect_resize(source: pathlib.Path, workdir: pathlib.Path) -> str:
    output = workdir / "resize_aspect.six"
    png = workdir / "resize_aspect.png"
    _, _, raster = encode_with_options(source, output, [
        (SIXEL_OPTFLAG_WIDTH, "48"),
        (SIXEL_OPTFLAG_RESAMPLING, "lanczos3"),
        (SIXEL_OPTFLAG_QUALITY, "auto"),
    ])
    source_size = read_png_dimensions(source)
    width, height = decode_to_png(output, png)
    if width != 48:
        raise SystemExit(f"expected width 48, got {width}")
    if not math.isclose(source_size[0] / source_size[1], width / height,
                        rel_tol=0.05, abs_tol=0.01):
        raise SystemExit("aspect ratio changed")
    if raster and (raster[0] > 1 or raster[1] > 1):
        if raster[0] != 48:
            raise SystemExit("raster width mismatch")
    return f"aspect preserved at 48px width (decoded {width}x{height})"


scenario = sys.argv[1]
source = pathlib.Path(sys.argv[2])
workdir = pathlib.Path(sys.argv[3])
workdir.mkdir(parents=True, exist_ok=True)

if scenario == "palette":
    print(run_palette_combo(source, workdir))
elif scenario == "resize_fixed":
    print(run_fixed_resize(source, workdir))
elif scenario == "resize_aspect":
    print(run_aspect_resize(source, workdir))
else:
    raise SystemExit(f"unknown scenario: {scenario}")
PY
)
    python_status=$?
    printf '%s' "${python_output}" >&2
    if [ "${python_status}" -eq 0 ]; then
        tap_pass ${case_id} "${description} via wheel"
    else
        python_skip_on_load_error "${python_status}" "${python_output}"
        tap_fail ${case_id} "${description} via wheel failed"
    fi

    case_id=$((case_id + 1))
}

run_case palette "palette/diffusion/quality options honor palette limit"
run_case resize_fixed "explicit resize dimensions survive decode"
run_case resize_aspect "width-only resize keeps aspect ratio"

tap_plan ${cases}
exit 0
