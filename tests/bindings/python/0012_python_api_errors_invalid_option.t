#!/bin/sh
# Validate Python encoder error handling for invalid option values.

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
    "${run_python}" - "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" "${ARTIFACT_LOCAL_DIR}/invalid_option" 2>&1 <<'PY'
import pathlib
import sys

try:
    from libsixel_wheel import (
        SIXEL_OPTFLAG_COLORS,
        SIXEL_OPTFLAG_INPUT,
        SIXEL_OPTFLAG_LOADERS,
        SIXEL_OPTFLAG_OUTPUT,
    )
    from libsixel_wheel.encoder import Encoder
except OSError as exc:
    print(f"SKIP_LIBSIXEL_LOAD:{exc}")
    raise SystemExit(2)

source = pathlib.Path(sys.argv[1])
workdir = pathlib.Path(sys.argv[2])
workdir.mkdir(parents=True, exist_ok=True)
target = workdir / "invalid-option.six"

message = ""
try:
    encoder = Encoder()
    encoder.setopt(SIXEL_OPTFLAG_LOADERS, "builtin!")
    encoder.setopt(SIXEL_OPTFLAG_COLORS, "-1")
    encoder.setopt(SIXEL_OPTFLAG_INPUT, str(source))
    encoder.setopt(SIXEL_OPTFLAG_OUTPUT, str(target))
    encoder.encode(str(source))
except Exception as exc:  # noqa: BLE001
    if not isinstance(exc, RuntimeError):
        raise SystemExit(
            f"invalid option value: expected RuntimeError, got {type(exc).__name__}"
        )
    message = str(exc) or "<empty message>"

if not message:
    raise SystemExit("invalid option value: expected exception but call succeeded")

keywords = (
    "invalid", "colors", "range", "parameter", "option",
    "bad argument", "value", "must"
)
if not any(keyword in message.lower() for keyword in keywords):
    raise SystemExit("invalid option value: missing expected keywords")

print(f"invalid option value: RuntimeError ({message})")
PY
)
python_status=$?
printf '%s' "${python_output}" >&2
test "${python_status}" -eq 0 && {
    pass 1 "invalid option value errors via wheel"
    tap_plan 1
    exit 0
}

marker=$(printf '%s' "${python_output}" | awk '/^SKIP_LIBSIXEL_LOAD:/{print; exit}')
test -n "${marker}" && skip_all "libsixel failed to load: ${marker#SKIP_LIBSIXEL_LOAD:}"&& skip_all "libsixel failed to load: ${marker#SKIP_LIBSIXEL_LOAD:}" {
    printf "1..0 # SKIP libsixel failed to load: ${marker#SKIP_LIBSIXEL_LOAD:}";
    exit 0
}

fail 1 "invalid option value errors via wheel failed"
tap_plan 1
exit 0
