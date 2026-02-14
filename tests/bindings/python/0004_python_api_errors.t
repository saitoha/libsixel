#!/bin/sh
# Validate Python encoder error handling for invalid inputs and options.

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

source_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
case_id=1

run_case() {
    scenario=$1
    description=$2
    working_dir="${ARTIFACT_LOCAL_DIR}/${scenario}"

    python_output=$(env \
        LIBSIXEL_LIBDIR="${libdir}" \
        LD_LIBRARY_PATH="${libdir}${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}" \
        DYLD_LIBRARY_PATH="${libdir}${DYLD_LIBRARY_PATH:+:${DYLD_LIBRARY_PATH}}" \
        "${run_python}" - "${scenario}" "${source_image}" \
        "${working_dir}" 2>&1 <<'PY'
import pathlib
import sys
from typing import Iterable

try:
    from libsixel_wheel import (
        SIXEL_OPTFLAG_COLORS,
        SIXEL_OPTFLAG_HEIGHT,
        SIXEL_OPTFLAG_INPUT,
        SIXEL_OPTFLAG_OUTPUT,
        SIXEL_OPTFLAG_WIDTH,
    )
    from libsixel_wheel.encoder import Encoder
except OSError as exc:
    print(f"SKIP_LIBSIXEL_LOAD:{exc}")
    raise SystemExit(2)


class Expectation:
    def __init__(self, label: str, exc_type: type[BaseException], keywords: Iterable[str]):
        self.label = label
        self.exc_type = exc_type
        self.keywords = [kw.lower() for kw in keywords]


def expect_exception(expectation: Expectation, func) -> str:
    try:
        func()
    except Exception as exc:  # noqa: BLE001
        if not isinstance(exc, expectation.exc_type):
            raise SystemExit(
                f"{expectation.label}: expected {expectation.exc_type.__name__}, "
                f"got {type(exc).__name__}"
            )
        message = str(exc) or "<empty message>"
        if expectation.keywords and not any(
            key in message.lower() for key in expectation.keywords
        ):
            raise SystemExit(f"{expectation.label}: missing expected keywords")
        return f"{expectation.label}: {expectation.exc_type.__name__} ({message})"
    raise SystemExit(f"{expectation.label}: expected exception but call succeeded")


def exercise_missing_path(workdir: pathlib.Path) -> str:
    missing = workdir / "does-not-exist.png"
    target = workdir / "missing.six"

    def _invoke() -> None:
        encoder = Encoder()
        encoder.setopt(SIXEL_OPTFLAG_INPUT, str(missing))
        encoder.setopt(SIXEL_OPTFLAG_OUTPUT, str(target))
        encoder.encode(str(missing))

    return expect_exception(
        Expectation("missing input", RuntimeError,
                    ("no such file", "cannot", "failed", "open",
                     "not found", "bad argument", "does not exist")),
        _invoke,
    )


def exercise_corrupted_image(workdir: pathlib.Path) -> str:
    broken_bmp = workdir / "broken.bmp"
    broken_bmp.write_bytes(b"BM\x00\x00")
    target = workdir / "broken.six"

    def _invoke() -> None:
        encoder = Encoder()
        encoder.setopt(SIXEL_OPTFLAG_INPUT, str(broken_bmp))
        encoder.setopt(SIXEL_OPTFLAG_OUTPUT, str(target))
        encoder.encode(str(broken_bmp))

    return expect_exception(
        Expectation("corrupted bmp", RuntimeError,
                    ("decode", "invalid", "corrupt", "format", "bmp",
                     "unable", "bad argument")),
        _invoke,
    )


def exercise_unsupported_format(workdir: pathlib.Path) -> str:
    text_file = workdir / "note.xxx"
    text_file.write_text("this is not an image")
    target = workdir / "note.six"

    def _invoke() -> None:
        encoder = Encoder()
        encoder.setopt(SIXEL_OPTFLAG_INPUT, str(text_file))
        encoder.setopt(SIXEL_OPTFLAG_OUTPUT, str(target))
        encoder.encode(str(text_file))

    return expect_exception(
        Expectation("unsupported format", RuntimeError,
                    ("unsupported", "decode", "format", "cannot", "failed",
                     "bad argument")),
        _invoke,
    )


def exercise_invalid_option(workdir: pathlib.Path, valid_source: pathlib.Path) -> str:
    target = workdir / "invalid-option.six"

    def _invoke() -> None:
        encoder = Encoder()
        encoder.setopt(SIXEL_OPTFLAG_COLORS, "-1")
        encoder.setopt(SIXEL_OPTFLAG_INPUT, str(valid_source))
        encoder.setopt(SIXEL_OPTFLAG_OUTPUT, str(target))
        encoder.encode(str(valid_source))

    return expect_exception(
        Expectation("invalid option value", RuntimeError,
                    ("invalid", "colors", "range", "parameter", "option",
                     "bad argument")),
        _invoke,
    )


scenario = sys.argv[1]
source = pathlib.Path(sys.argv[2])
workdir = pathlib.Path(sys.argv[3])
workdir.mkdir(parents=True, exist_ok=True)

if scenario == "missing":
    print(exercise_missing_path(workdir))
elif scenario == "corrupt":
    print(exercise_corrupted_image(workdir))
elif scenario == "unsupported":
    print(exercise_unsupported_format(workdir))
elif scenario == "invalid_option":
    print(exercise_invalid_option(workdir, source))
else:
    raise SystemExit(f"unknown scenario: {scenario}")
PY
)
    python_status=$?
    printf '%s' "${python_output}" >&2
    if [ "${python_status}" -eq 0 ]; then
        tap_pass ${case_id} "${description} via wheel"
    else
        python_skip_on_load_error "${python_status}" "${python_output}"
        tap_fail ${case_id} "${description} via wheel failed"
    fi

    case_id=$((case_id + 1))
}

run_case missing "missing input path errors"
run_case corrupt "corrupted image errors"
run_case unsupported "unsupported format errors"
run_case invalid_option "invalid option value errors"

tap_plan 4
exit 0
