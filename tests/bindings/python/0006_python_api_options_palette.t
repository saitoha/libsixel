#!/bin/sh
# Exercise palette, diffusion, and quality options.

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
    "${ARTIFACT_LOCAL_DIR}/palette" 2>&1 <<'PY'
import pathlib
import re
import sys

try:
    from libsixel_wheel import (
        SIXEL_OPTFLAG_BGCOLOR,
        SIXEL_OPTFLAG_COLORS,
        SIXEL_OPTFLAG_DIFFUSION,
        SIXEL_OPTFLAG_INPUT,
        SIXEL_OPTFLAG_OUTPUT,
        SIXEL_OPTFLAG_PALETTE_TYPE,
        SIXEL_OPTFLAG_QUALITY,
    )
    from libsixel_wheel.encoder import Encoder
except OSError as exc:
    print(f"SKIP_LIBSIXEL_LOAD:{exc}")
    raise SystemExit(2)

source = pathlib.Path(sys.argv[1])
workdir = pathlib.Path(sys.argv[2])
workdir.mkdir(parents=True, exist_ok=True)
output = workdir / "palette.six"

encoder = Encoder()
encoder.setopt(SIXEL_OPTFLAG_INPUT, str(source))
encoder.setopt(SIXEL_OPTFLAG_OUTPUT, str(output))
encoder.setopt(SIXEL_OPTFLAG_COLORS, "16")
encoder.setopt(SIXEL_OPTFLAG_DIFFUSION, "atkinson")
encoder.setopt(SIXEL_OPTFLAG_PALETTE_TYPE, "hls")
encoder.setopt(SIXEL_OPTFLAG_QUALITY, "high")
encoder.setopt(SIXEL_OPTFLAG_BGCOLOR, "#000000")
encoder.encode(str(source))

if not output.exists() or output.stat().st_size == 0:
    raise SystemExit("missing or empty sixel output")

data = output.read_bytes()
if not data.startswith(b"\x1bPq"):
    raise SystemExit("missing sixel DCS introducer")
if not data.rstrip(b"\r\n").endswith(b"\x1b\\"):
    raise SystemExit("missing sixel ST terminator")

palette = {int(entry) for entry in re.findall(rb"#(\d+)", data)}
if not palette or len(palette) > 16:
    raise SystemExit("palette limit check failed")

raster = re.search(rb'"(\d+);(\d+);(\d+);(\d+)', data)
if raster:
    print(f"palette ok (<=16 entries), raster={raster.group(1).decode()}x{raster.group(2).decode()}")
else:
    print("palette ok (<=16 entries)")
PY
)
python_status=$?
printf '%s' "${python_output}" >&2
test "${python_status}" -eq 0 && {
    pass 1 "palette/diffusion/quality options honor palette limit via wheel"
    tap_plan 1
    exit 0
}

marker=$(printf '%s' "${python_output}" | awk '/^SKIP_LIBSIXEL_LOAD:/{print; exit}')
test -n "${marker}" && skip_all "libsixel failed to load: ${marker#SKIP_LIBSIXEL_LOAD:}"

fail 1 "palette/diffusion/quality options via wheel failed"
tap_plan 1
exit 0
