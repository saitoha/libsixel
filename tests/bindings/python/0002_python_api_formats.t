#!/bin/sh
# Verify Python bindings can encode multiple image formats and emit
# well-formed SIXEL streams.

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
    test "${python_status}" -eq 0 && {
        tap_pass ${case_id} "encodes ${label} via wheel (DCS/ST ok)"
        case_id=$((case_id + 1))
        continue
    }

    marker=$(printf '%s' "${python_output}" | awk '/^SKIP_LIBSIXEL_LOAD:/{print; exit}')
    test -n "${marker}" &&         tap_skip_all "libsixel failed to load: ${marker#SKIP_LIBSIXEL_LOAD:}"

    tap_fail ${case_id} "${label} encoding via wheel failed"
    case_id=$((case_id + 1))
done <<EOF2
PNG:${TOP_SRCDIR}/tests/data/inputs/snake_64.png
JPEG:${TOP_SRCDIR}/tests/data/inputs/snake_64.jpg
GIF:${TOP_SRCDIR}/tests/data/inputs/snake_64.gif
BMP:${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp
EOF2

tap_plan ${formats_count}
exit 0
