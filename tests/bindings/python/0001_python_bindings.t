#!/bin/sh
# TAP smoke test for Python bindings using the shared test virtualenv.

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

    marker=$(printf '%s' "${log_text}" | awk '/^SKIP_LIBSIXEL_LOAD:/{print; exit}')
    if [ -n "${marker}" ]; then
        tap_skip_all "libsixel failed to load: ${marker#SKIP_LIBSIXEL_LOAD:}"
    fi
}

case_id=1
python_output=$(env \
    LIBSIXEL_LIBDIR="${libdir}" \
    LD_LIBRARY_PATH="${libdir}${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}" \
    DYLD_LIBRARY_PATH="${libdir}${DYLD_LIBRARY_PATH:+:${DYLD_LIBRARY_PATH}}" \
    "${run_python}" - "${ARTIFACT_LOCAL_DIR}" 2>&1 <<'PY'
import ctypes.util
import glob
import os
import pathlib
import sys


def _prefer_build_library(name, original_find):
    libdir = os.environ.get("LIBSIXEL_LIBDIR")
    if libdir:
        prefixes = ["lib", ""]
        suffixes = [".so", ".dylib", ".dll"]
        for prefix in prefixes:
            for suffix in suffixes:
                pattern = os.path.join(libdir, f"{prefix}{name}*{suffix}")
                matches = sorted(glob.glob(pattern))
                if matches:
                    return matches[0]
    return original_find(name)


ctypes.util.find_library = (
    lambda name, _orig=ctypes.util.find_library: _prefer_build_library(name, _orig)
)

try:
    from libsixel_wheel import SIXEL_PIXELFORMAT_RGB888
    from libsixel_wheel.encoder import Encoder, SIXEL_OPTFLAG_OUTPUT

    root = pathlib.Path(sys.argv[1])
    output = root / "sample.six"

    pixels = bytes([
        255, 0, 0,
        0, 255, 0,
        0, 0, 255,
        255, 255, 255,
    ])

    encoder = Encoder()
    encoder.setopt(SIXEL_OPTFLAG_OUTPUT, str(output))
    for _ in range(2):
        encoder.encode_bytes(pixels, 2, 2, SIXEL_PIXELFORMAT_RGB888, None)

    if not output.exists() or output.stat().st_size == 0:
        raise SystemExit("missing or empty sixel output")

    print("encode succeeded")
except OSError as exc:
    print(f"SKIP_LIBSIXEL_LOAD:{exc}")
    raise SystemExit(2)
PY
)
python_status=$?
printf '%s' "${python_output}" >&2
if [ "${python_status}" -eq 0 ]; then
    tap_pass ${case_id} "encodes image via wheel"
else
    python_skip_on_load_error "${python_status}" "${python_output}"
    tap_fail ${case_id} "python wheel round-trip failed"
fi

tap_plan 1
exit 0
