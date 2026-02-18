#!/bin/sh
# Validate Python encoder error handling for unsupported formats.

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
    "${run_python}" - "${ARTIFACT_LOCAL_DIR}/unsupported" 2>&1 <<'PY'
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
text_file = workdir / "note.xxx"
text_file.write_text("this is not an image")
target = workdir / "note.six"

message = ""
try:
    encoder = Encoder()
    encoder.setopt(SIXEL_OPTFLAG_LOADERS, "builtin!")
    encoder.setopt(SIXEL_OPTFLAG_INPUT, str(text_file))
    encoder.setopt(SIXEL_OPTFLAG_OUTPUT, str(target))
    encoder.encode(str(text_file))
except Exception as exc:  # noqa: BLE001
    if not isinstance(exc, RuntimeError):
        raise SystemExit(
            f"unsupported format: expected RuntimeError, got {type(exc).__name__}"
        )
    message = str(exc) or "<empty message>"

if not message:
    raise SystemExit("unsupported format: expected exception but call succeeded")

keywords = (
    "unsupported", "decode", "format", "cannot", "failed",
    "bad argument", "error", "stb_image"
)
if not any(keyword in message.lower() for keyword in keywords):
    raise SystemExit("unsupported format: missing expected keywords")

print(f"unsupported format: RuntimeError ({message})")
PY
)
python_status=$?
printf '%s' "${python_output}" >&2
test "${python_status}" -eq 0 && {
    printf '1..1\n'
    pass 1 "unsupported format errors via wheel"
    exit 0
}

marker=$(printf '%s' "${python_output}" | awk '/^SKIP_LIBSIXEL_LOAD:/{print; exit}')
test -n "${marker}" && {
    printf "1..0 # SKIP libsixel failed to load: ${marker#SKIP_LIBSIXEL_LOAD:}";
    exit 0
}

printf '1..1\n'
fail 1 "unsupported format errors via wheel failed"
exit 0
