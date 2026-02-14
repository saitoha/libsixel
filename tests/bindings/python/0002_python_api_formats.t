#!/bin/sh
# Verify Python bindings can encode multiple image formats and emit
# well-formed SIXEL streams.

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

formats_count=4
case_id=1

while IFS=: read -r label source_path; do
    output_path="${ARTIFACT_LOCAL_DIR}/${label}.six"

    python_output=$(env \
        LIBSIXEL_LIBDIR="${libdir}" \
        LD_LIBRARY_PATH="${libdir}${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}" \
        DYLD_LIBRARY_PATH="${libdir}${DYLD_LIBRARY_PATH:+:${DYLD_LIBRARY_PATH}}" \
        "${run_python}" - "${source_path}" "${output_path}" 2>&1 <<'PY'
import pathlib
import sys

try:
    from libsixel_wheel import SIXEL_OPTFLAG_INPUT
    from libsixel_wheel.encoder import Encoder, SIXEL_OPTFLAG_OUTPUT

    def ensure_sixel_signature(path: pathlib.Path) -> int:
        data = path.read_bytes()
        if not data.startswith(b"\x1bPq"):
            raise SystemExit("missing sixel DCS introducer")
        if not data.rstrip(b"\r\n").endswith(b"\x1b\\"):
            raise SystemExit("missing sixel ST terminator")
        return len(data)

    source = pathlib.Path(sys.argv[1])
    target = pathlib.Path(sys.argv[2])

    encoder = Encoder()
    encoder.setopt(SIXEL_OPTFLAG_INPUT, str(source))
    encoder.setopt(SIXEL_OPTFLAG_OUTPUT, str(target))
    encoder.encode(str(source))

    if not target.exists() or target.stat().st_size == 0:
        raise SystemExit("missing or empty sixel output")

    size = ensure_sixel_signature(target)
    print(f"encoded {source.name} -> {target.name} ({size} bytes)")
except OSError as exc:
    print(f"SKIP_LIBSIXEL_LOAD:{exc}")
    raise SystemExit(2)
PY
)
    python_status=$?
    printf '%s' "${python_output}" >&2
    if [ "${python_status}" -eq 0 ]; then
        tap_pass ${case_id} "encodes ${label} via wheel (DCS/ST ok)"
    else
        python_skip_on_load_error "${python_status}" "${python_output}"
        tap_fail ${case_id} "${label} encoding via wheel failed"
    fi

    case_id=$((case_id + 1))
done <<EOF2
PNG:${TOP_SRCDIR}/tests/data/inputs/snake_64.png
JPEG:${TOP_SRCDIR}/tests/data/inputs/snake_64.jpg
GIF:${TOP_SRCDIR}/tests/data/inputs/snake_64.gif
BMP:${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp
EOF2

tap_plan ${formats_count}
exit 0
