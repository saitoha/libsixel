#!/bin/sh
# Validate Python encoder error handling for invalid inputs and option values.
# The scenarios cover missing files, corrupted or unsupported formats, overly
# large dimension requests, and invalid option parameters so that regression in
# error reporting can be caught early.

set -eu

. "$(dirname "$0")/../lib/common.sh"

test_name=$(basename "$0")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${test_name}"
log_file="${artifact_dir}/python.log"
tmp_dir="${artifact_dir}/tmp"

# Resolve the fixture relative to the repository root so the Python preflight
# checks see an absolute path even when Automake does not populate TOP_SRCDIR.
source_image=$(cd "$(dirname "$0")/../../.." && pwd)/images/autumn.png

mkdir -p "${artifact_dir}" "${tmp_dir}"
: >"${log_file}"

printf 'source_image=%s\n' "${source_image}" >>"${log_file}"

tap_log_file="${log_file}"

python_prepare "${log_file}" "${tmp_dir}"

verify_script="${tmp_dir}/verify-errors.py"
cat >"${verify_script}" <<'PY'
"""Exercise Python encoder error handling cases.

Each helper raises SystemExit with a descriptive message when the observed
exception does not match expectations, keeping the TAP harness concise while
surfacing detailed failure reasons.
"""
import pathlib
import sys
from typing import Iterable

from libsixel import (
    SIXEL_OPTFLAG_COLORS,
    SIXEL_OPTFLAG_HEIGHT,
    SIXEL_OPTFLAG_INPUT,
    SIXEL_OPTFLAG_OUTPUT,
    SIXEL_OPTFLAG_WIDTH,
)
from libsixel.encoder import Encoder


class Expectation:
    """Bundle expectation parameters for readability."""

    def __init__(self, label: str, exc_type: type[BaseException], keywords: Iterable[str]):
        self.label = label
        self.exc_type = exc_type
        self.keywords = [kw.lower() for kw in keywords]


def expect_exception(expectation: Expectation, func) -> str:
    """Run func and validate it raises the expected exception type/message."""

    try:
        func()
    except Exception as exc:  # noqa: BLE001 - explicit type validation below
        if not isinstance(exc, expectation.exc_type):
            raise SystemExit(
                f"{expectation.label}: expected {expectation.exc_type.__name__}, "
                f"got {type(exc).__name__}"
            )

        message = str(exc) or "<empty message>"
        if expectation.keywords and not any(
            key in message.lower() for key in expectation.keywords
        ):
            raise SystemExit(
                f"{expectation.label}: message did not mention {expectation.keywords}: {message}"
            )

        return f"{expectation.label}: {expectation.exc_type.__name__} ({message})"

    raise SystemExit(f"{expectation.label}: expected exception but call succeeded")


def exercise_missing_path(workdir: pathlib.Path) -> str:
    """Expect a missing input to raise a RuntimeError with file hints."""

    workdir.mkdir(parents=True, exist_ok=True)
    missing = workdir / "does-not-exist.png"
    target = workdir / "missing.six"

    def _invoke() -> None:
        encoder = Encoder()
        encoder.setopt(SIXEL_OPTFLAG_INPUT, str(missing))
        encoder.setopt(SIXEL_OPTFLAG_OUTPUT, str(target))
        encoder.encode(str(missing))

    expectation = Expectation(
        "missing input",
        RuntimeError,
        (
            "no such file",
            "cannot",
            "failed",
            "open",
            "not found",
            "bad argument",
            "does not exist",
        ),
    )
    return expect_exception(expectation, _invoke)


def exercise_corrupted_image(workdir: pathlib.Path) -> str:
    """Expect a malformed BMP to be rejected without crashing."""

    workdir.mkdir(parents=True, exist_ok=True)
    broken_bmp = workdir / "broken.bmp"
    broken_bmp.write_bytes(b"BM\x00\x00")
    target = workdir / "broken.six"

    def _invoke() -> None:
        encoder = Encoder()
        encoder.setopt(SIXEL_OPTFLAG_INPUT, str(broken_bmp))
        encoder.setopt(SIXEL_OPTFLAG_OUTPUT, str(target))
        encoder.encode(str(broken_bmp))

    expectation = Expectation(
        "corrupted bmp",
        RuntimeError,
        ("decode", "invalid", "corrupt", "format", "bmp", "unable", "bad argument"),
    )
    return expect_exception(expectation, _invoke)


def exercise_unsupported_format(workdir: pathlib.Path) -> str:
    """Expect a plain text input to be rejected as unsupported."""

    workdir.mkdir(parents=True, exist_ok=True)
    text_file = workdir / "note.xxx"
    text_file.write_text("this is not an image")
    target = workdir / "note.six"

    def _invoke() -> None:
        encoder = Encoder()
        encoder.setopt(SIXEL_OPTFLAG_INPUT, str(text_file))
        encoder.setopt(SIXEL_OPTFLAG_OUTPUT, str(target))
        encoder.encode(str(text_file))

    expectation = Expectation(
        "unsupported format",
        RuntimeError,
        ("unsupported", "decode", "format", "cannot", "failed", "bad argument"),
    )
    return expect_exception(expectation, _invoke)


def exercise_oversized_dimensions(workdir: pathlib.Path, source: pathlib.Path) -> str:
    """Expect extremely large geometry requests to raise RuntimeError."""

    workdir.mkdir(parents=True, exist_ok=True)
    target = workdir / "oversized.six"
    huge_dimension = 20000

    def _invoke() -> None:
        encoder = Encoder()
        encoder.setopt(SIXEL_OPTFLAG_INPUT, str(source))
        encoder.setopt(SIXEL_OPTFLAG_OUTPUT, str(target))
        encoder.setopt(SIXEL_OPTFLAG_WIDTH, str(huge_dimension))
        encoder.setopt(SIXEL_OPTFLAG_HEIGHT, str(huge_dimension))
        encoder.encode(str(source))

    expectation = Expectation(
        "oversized dimensions",
        RuntimeError,
        ("alloc", "memory", "bad", "failed"),
    )
    return expect_exception(expectation, _invoke)


def exercise_invalid_option(workdir: pathlib.Path, valid_source: pathlib.Path) -> str:
    """Expect invalid option values to raise RuntimeError with context."""

    workdir.mkdir(parents=True, exist_ok=True)
    target = workdir / "invalid-option.six"

    def _invoke() -> None:
        encoder = Encoder()
        encoder.setopt(SIXEL_OPTFLAG_COLORS, "-1")
        encoder.setopt(SIXEL_OPTFLAG_INPUT, str(valid_source))
        encoder.setopt(SIXEL_OPTFLAG_OUTPUT, str(target))
        encoder.encode(str(valid_source))

    expectation = Expectation(
        "invalid option value",
        RuntimeError,
        ("invalid", "colors", "range", "parameter", "option", "bad argument"),
    )
    return expect_exception(expectation, _invoke)


def main() -> None:
    if len(sys.argv) != 3:
        raise SystemExit("usage: verify-errors.py <source-image> <workdir>")

    source = pathlib.Path(sys.argv[1])
    workdir = pathlib.Path(sys.argv[2])
    workdir.mkdir(parents=True, exist_ok=True)

    results = [
        exercise_missing_path(workdir / "missing"),
        exercise_corrupted_image(workdir / "corrupt"),
        exercise_unsupported_format(workdir / "unsupported"),
        exercise_oversized_dimensions(workdir / "oversized", source),
        exercise_invalid_option(workdir / "options", source),
    ]

    for entry in results:
        print(entry)


if __name__ == "__main__":
    main()
PY

# Install the wheel into a venv when available so the API is exercised through
# the packaged module instead of the in-tree sources.
if [ "${use_wheel}" -eq 1 ]; then
    run_venv="${tmp_dir}/venv"
    if ! python_install_wheel "${run_venv}" "${wheel_path}"; then
        tap_skip_all "wheel installation failed"
    fi
fi

tap_plan 5
case_id=1

run_case() {
    scenario=$1
    description=$2

    working_dir="${tmp_dir}/${scenario}"

    mkdir -p "${working_dir}" "${working_dir}/wheel" "${working_dir}/tree"

    if [ "${use_wheel}" -eq 1 ]; then
        ld_env="${python_wheel_ld_library_path}"

        if PYTHONPATH="" \
           LD_LIBRARY_PATH="${ld_env}" \
           LIBSIXEL_LIBDIR="${lib_dir}" \
           "${run_python}" "${verify_script}" \
           "${source_image}" "${working_dir}/wheel" \
           >>"${log_file}" 2>&1; then
            tap_pass ${case_id} "${description} via wheel"
        else
            tap_fail ${case_id} "${description} via wheel failed"
        fi
    else
        if PYTHONPATH="${python_in_tree_pythonpath}" \
           LD_LIBRARY_PATH="${python_in_tree_ld_library_path}" \
           LIBSIXEL_LIBDIR="${lib_dir}" \
           "${run_python}" "${verify_script}" \
           "${source_image}" "${working_dir}/tree" \
           >>"${log_file}" 2>&1; then
            tap_pass ${case_id} "${description} via in-tree modules"
        else
            tap_fail ${case_id} "${description} via in-tree modules failed"
        fi
    fi

    case_id=$((case_id + 1))
}

run_case missing "missing input path errors"
run_case corrupt "corrupted image errors"
run_case unsupported "unsupported format errors"
run_case oversized "oversized encode_bytes errors"
run_case invalid_option "invalid option value errors"

exit ${tap_status}
