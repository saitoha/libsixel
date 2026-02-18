#!/bin/sh
# Verify Python bindings can encode BMP input and emit a well-formed SIXEL stream.

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

output_path="${ARTIFACT_LOCAL_DIR}/BMP.six"
python_output=$(env \
    LIBSIXEL_LIBDIR="${libdir}" \
    LD_LIBRARY_PATH="${libdir}${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}" \
    DYLD_LIBRARY_PATH="${libdir}${DYLD_LIBRARY_PATH:+:${DYLD_LIBRARY_PATH}}" \
    "${run_python}" - "${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp" "${output_path}" 2>&1 <<'PY'
import pathlib
import sys

try:
    from libsixel_wheel import SIXEL_OPTFLAG_INPUT
    from libsixel_wheel.encoder import Encoder, SIXEL_OPTFLAG_OUTPUT

    source = pathlib.Path(sys.argv[1])
    target = pathlib.Path(sys.argv[2])

    encoder = Encoder()
    encoder.setopt(SIXEL_OPTFLAG_INPUT, str(source))
    encoder.setopt(SIXEL_OPTFLAG_OUTPUT, str(target))
    encoder.encode(str(source))

    if not target.exists() or target.stat().st_size == 0:
        raise SystemExit("missing or empty sixel output")

    data = target.read_bytes()
    if not data.startswith(b"\x1bPq"):
        raise SystemExit("missing sixel DCS introducer")
    if not data.rstrip(b"\r\n").endswith(b"\x1b\\"):
        raise SystemExit("missing sixel ST terminator")

    print(f"encoded {source.name} -> {target.name} ({len(data)} bytes)")
except OSError as exc:
    print(f"SKIP_LIBSIXEL_LOAD:{exc}")
    raise SystemExit(2)
PY
)
python_status=$?
printf '%s' "${python_output}" >&2
test "${python_status}" -eq 0 && {
    tap_plan 1
    pass 1 "encodes BMP via wheel (DCS/ST ok)"
    exit 0
}

marker=$(printf '%s' "${python_output}" | awk '/^SKIP_LIBSIXEL_LOAD:/{print; exit}')
test -n "${marker}" && {
    printf "1..0 # SKIP libsixel failed to load: ${marker#SKIP_LIBSIXEL_LOAD:}";
    exit 0
}

tap_plan 1
fail 1 "BMP encoding via wheel failed"
exit 0
