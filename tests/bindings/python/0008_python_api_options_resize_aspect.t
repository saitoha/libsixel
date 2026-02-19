#!/bin/sh
# Exercise width-only resize while preserving aspect ratio.

set -eux

test "${ENABLE_PYTHON:-0}" = "1" || {
    printf "1..0 # SKIP python bindings are disabled in this build\n"
    exit 0
}
test -n "${SIXEL_TEST_PYTHON_VENV:-}" || {
    printf "1..0 # SKIP python wheel test environment is unavailable\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

run_python="${SIXEL_TEST_PYTHON_VENV}/bin/python"
libdir="${LIBSIXEL_LIBDIR:-${TOP_BUILDDIR}/src/.libs}"
test -d "${libdir}" || libdir="${TOP_BUILDDIR}/src"

python_output=$(env \
    LIBSIXEL_LIBDIR="${libdir}" \
    LD_LIBRARY_PATH="${libdir}${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}" \
    DYLD_LIBRARY_PATH="${libdir}${DYLD_LIBRARY_PATH:+:${DYLD_LIBRARY_PATH}}" \
    "${run_python}" - \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" \
    "${ARTIFACT_LOCAL_DIR}/resize_aspect" 2>&1 <<'PY'
import math
import pathlib
import re
import sys

try:
    from libsixel_wheel import (
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

source = pathlib.Path(sys.argv[1])
workdir = pathlib.Path(sys.argv[2])
workdir.mkdir(parents=True, exist_ok=True)
output = workdir / "resize_aspect.six"
png = workdir / "resize_aspect.png"

source_header = source.read_bytes()
if len(source_header) < 24 or source_header[:8] != b"\x89PNG\r\n\x1a\n":
    raise SystemExit("source is not a PNG")
source_width = int.from_bytes(source_header[16:20], "big")
source_height = int.from_bytes(source_header[20:24], "big")

encoder = Encoder()
encoder.setopt(SIXEL_OPTFLAG_INPUT, str(source))
encoder.setopt(SIXEL_OPTFLAG_OUTPUT, str(output))
encoder.setopt(SIXEL_OPTFLAG_WIDTH, "48")
encoder.setopt(SIXEL_OPTFLAG_RESAMPLING, "lanczos3")
encoder.setopt(SIXEL_OPTFLAG_QUALITY, "auto")
encoder.encode(str(source))

if not output.exists() or output.stat().st_size == 0:
    raise SystemExit("missing or empty sixel output")

data = output.read_bytes()
if not data.startswith(b"\x1bPq"):
    raise SystemExit("missing sixel DCS introducer")
if not data.rstrip(b"\r\n").endswith(b"\x1b\\"):
    raise SystemExit("missing sixel ST terminator")

raster = re.search(rb'"(\d+);(\d+);(\d+);(\d+)', data)

decoder = Decoder()
decoder.setopt(SIXEL_OPTFLAG_INPUT, str(output))
decoder.setopt(SIXEL_OPTFLAG_OUTPUT, str(png))
decoder.decode(str(output))
if not png.exists() or png.stat().st_size == 0:
    raise SystemExit("decoder did not write output")

header = png.read_bytes()
if len(header) < 24 or header[:8] != b"\x89PNG\r\n\x1a\n":
    raise SystemExit("output is not a PNG")
width = int.from_bytes(header[16:20], "big")
height = int.from_bytes(header[20:24], "big")

if width != 48:
    raise SystemExit(f"expected width 48, got {width}")
if not math.isclose(source_width / source_height, width / height,
                    rel_tol=0.05, abs_tol=0.01):
    raise SystemExit("aspect ratio changed")
if raster and (int(raster.group(1)) > 1 or int(raster.group(2)) > 1):
    if int(raster.group(1)) != 48:
        raise SystemExit("raster width mismatch")

print(f"aspect preserved at 48px width (decoded {width}x{height})")
PY
)
python_status=$?
printf '%s' "${python_output}" >&2
test "${python_status}" -eq 0 && {
    printf '1..1\n'
    pass 1 "width-only resize keeps aspect ratio via wheel"
    exit 0
}

marker=$(printf '%s' "${python_output}" | awk '/^SKIP_LIBSIXEL_LOAD:/{print; exit}')
test -n "${marker}" && {
    printf "1..0 # SKIP %s\n" "libsixel failed to load: ${marker#SKIP_LIBSIXEL_LOAD:}";
    exit 0
}

printf '1..1\n'
fail 1 "width-only resize via wheel failed"
exit 0
