#!/bin/sh
# Validate Python encoder error handling for corrupted images.

set -eux

test "${ENABLE_PYTHON:-0}" = "1" || {
    printf "1..0 # SKIP python bindings are disabled in this build"
    exit 0
}
test -n "${SIXEL_TEST_PYTHON_VENV:-}" || {
    printf "1..0 # SKIP python wheel test environment is unavailable"
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
    "${run_python}" - "${ARTIFACT_LOCAL_DIR}/corrupt" 2>&1 <<'PY'
import pathlib
import sys

try:
    from libsixel_wheel import (
        SIXEL_OPTFLAG_INPUT,
        SIXEL_OPTFLAG_LOADERS,
        SIXEL_OPTFLAG_OUTPUT,
    )
    from libsixel_wheel.encoder import Encoder
except OSError as exc:
    print(f"SKIP_LIBSIXEL_LOAD:{exc}")
    raise SystemExit(2)

workdir = pathlib.Path(sys.argv[1])
workdir.mkdir(parents=True, exist_ok=True)
broken_bmp = workdir / "broken.bmp"
broken_bmp.write_bytes(b"BM\x00\x00")
target = workdir / "broken.six"

message = ""
try:
    encoder = Encoder()
    encoder.setopt(SIXEL_OPTFLAG_LOADERS, "builtin!")
    encoder.setopt(SIXEL_OPTFLAG_INPUT, str(broken_bmp))
    encoder.setopt(SIXEL_OPTFLAG_OUTPUT, str(target))
    encoder.encode(str(broken_bmp))
except Exception as exc:  # noqa: BLE001
    if not isinstance(exc, RuntimeError):
        raise SystemExit(f"corrupted bmp: expected RuntimeError, got {type(exc).__name__}")
    message = str(exc) or "<empty message>"

if not message:
    raise SystemExit("corrupted bmp: expected exception but call succeeded")

print(f"corrupted bmp: RuntimeError ({message})")
PY
)
python_status=$?
printf '%s' "${python_output}" >&2
test "${python_status}" -eq 0 && {
    printf '1..1\n'
    pass 1 "corrupted image errors via wheel"
    exit 0
}

marker=$(printf '%s' "${python_output}" | awk '/^SKIP_LIBSIXEL_LOAD:/{print; exit}')
test -n "${marker}" && {
    printf "1..0 # SKIP libsixel failed to load: ${marker#SKIP_LIBSIXEL_LOAD:}";
    exit 0
}

printf '1..1\n'
fail 1 "corrupted image errors via wheel failed"
exit 0
