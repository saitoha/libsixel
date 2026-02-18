#!/bin/sh
# Ensure resource cleanup for Python encode/decode workflows.

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

source_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
work_dir="${ARTIFACT_LOCAL_DIR}/work"

python_output=$(env \
    LIBSIXEL_LIBDIR="${libdir}" \
    LD_LIBRARY_PATH="${libdir}${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}" \
    DYLD_LIBRARY_PATH="${libdir}${DYLD_LIBRARY_PATH:+:${DYLD_LIBRARY_PATH}}" \
    "${run_python}" - "${source_image}" "${work_dir}" 2>&1 <<'PY'
import gc
import pathlib
import sys
import time
import warnings

try:
    from libsixel_wheel import (
        SIXEL_OPTFLAG_INPUT,
        SIXEL_OPTFLAG_OUTPUT,
        SIXEL_OPTFLAG_WIDTH,
        SIXEL_OPTFLAG_HEIGHT,
    )
    from libsixel_wheel.encoder import Encoder
    from libsixel_wheel.decoder import Decoder
except OSError as exc:
    print(f"SKIP_LIBSIXEL_LOAD:{exc}")
    raise SystemExit(2)

source = pathlib.Path(sys.argv[1])
workdir = pathlib.Path(sys.argv[2])
workdir.mkdir(parents=True, exist_ok=True)

sixel_path = workdir / "large.six"
decoded_png = workdir / "roundtrip.png"

with warnings.catch_warnings():
    warnings.simplefilter("error", ResourceWarning)

    with Encoder() as encoder:
        encoder.setopt(SIXEL_OPTFLAG_INPUT, str(source))
        encoder.setopt(SIXEL_OPTFLAG_OUTPUT, str(sixel_path))
        encoder.setopt(SIXEL_OPTFLAG_WIDTH, "800")
        encoder.setopt(SIXEL_OPTFLAG_HEIGHT, "600")
        encoder.encode(str(source))

    if not sixel_path.exists() or sixel_path.stat().st_size == 0:
        raise SystemExit("encoder output missing or empty")
    size = sixel_path.stat().st_size

    with Decoder() as decoder:
        decoder.setopt(SIXEL_OPTFLAG_INPUT, str(sixel_path))
        decoder.setopt(SIXEL_OPTFLAG_OUTPUT, str(decoded_png))
        decoder.decode(str(sixel_path))

    if not decoded_png.exists() or decoded_png.stat().st_size == 0:
        raise SystemExit("decoder output missing or empty")
    header = decoded_png.read_bytes()[:24]
    if len(header) < 24:
        raise SystemExit("decoded PNG header too small")
    width = int.from_bytes(header[16:20], "big")
    height = int.from_bytes(header[20:24], "big")

limit = time.monotonic() + 15.0
while True:
    gc.collect()
    try:
        sixel_path.unlink()
        break
    except FileNotFoundError:
        break
    except PermissionError:
        if time.monotonic() >= limit:
            raise SystemExit(f"failed to remove {sixel_path.name}")
        time.sleep(0.2)

limit = time.monotonic() + 15.0
while True:
    gc.collect()
    try:
        decoded_png.unlink()
        break
    except FileNotFoundError:
        break
    except PermissionError:
        if time.monotonic() >= limit:
            raise SystemExit(f"failed to remove {decoded_png.name}")
        time.sleep(0.2)

if sixel_path.exists() or decoded_png.exists():
    raise SystemExit("output files persist after deletion")

print(f"encoded {size} bytes, decoded {width}x{height}, resources cleaned")
PY
)
python_status=$?
printf '%s' "${python_output}" >&2

test "${python_status}" -eq 0 && {
    tap_plan 1
    pass 1 "large image roundtrip via wheel frees resources"
    exit 0
}

marker=$(printf '%s' "${python_output}" | awk '/^SKIP_LIBSIXEL_LOAD:/{print; exit}')
test -n "${marker}" && skip_all "libsixel failed to load: ${marker#SKIP_LIBSIXEL_LOAD:}"

tap_plan 1
fail 1 "resource test via wheel failed"
exit 0
