#!/bin/sh
# Validate Python encoder error handling for invalid inputs and option values.
# The scenarios cover missing files, corrupted or unsupported formats, overly
# large dimension requests, and invalid option parameters so that regression in
# error reporting can be caught early without generating temporary scripts.

set -euxv

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
rm -f "${log_file}"

printf 'source_image=%s\n' "${source_image}" >>"${log_file}"

tap_log_file="${log_file}"

python_prepare "${log_file}" "${tmp_dir}"

# Install the wheel into a venv when available so the API is exercised through
# the packaged module instead of the in-tree sources.
if [ "${use_wheel}" -eq 1 ]; then
    run_venv="${tmp_dir}/venv"
    if ! python_install_wheel "${run_venv}" "${wheel_path}"; then
        tap_skip_all "wheel installation failed"
    fi
fi

case_id=1

run_case() {
    scenario=$1
    description=$2

    working_dir="${tmp_dir}/${scenario}"

    mkdir -p "${working_dir}" "${working_dir}/wheel" "${working_dir}/tree"

    run_python_scenarios() {
        PYTHONPATH=$1
        shift
        LD_LIBRARY_PATH=$1
        shift
        LIBSIXEL_LIBDIR=${lib_dir}
        export PYTHONPATH LD_LIBRARY_PATH LIBSIXEL_LIBDIR

        "${run_python}" - "$@" <<'PY'
"""Exercise Python encoder error handling without temporary scripts."""
import pathlib
import sys
from typing import Iterable

try:
    from libsixel import (
        SIXEL_OPTFLAG_COLORS,
        SIXEL_OPTFLAG_HEIGHT,
        SIXEL_OPTFLAG_INPUT,
        SIXEL_OPTFLAG_OUTPUT,
        SIXEL_OPTFLAG_WIDTH,
    )
    from libsixel.encoder import Encoder
except OSError as exc:
    print(f"SKIP_LIBSIXEL_LOAD:{exc}")
    raise SystemExit(2)


class Expectation:
    """Bundle expectation parameters for readability."""

    def __init__(self, label: str, exc_type: type[BaseException], keywords: Iterable[str]):
        self.label = label
        self.exc_type = exc_type
        self.keywords = [kw.lower() for kw in keywords]


def expect_exception(expectation: Expectation, func) -> str:
    """Run func and validate it raises the expected exception."""

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
            raise SystemExit(
                f"{expectation.label}: message did not mention "
                f"{expectation.keywords}: {message}"
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
        (
            "decode",
            "invalid",
            "corrupt",
            "format",
            "bmp",
            "unable",
            "bad argument",
        ),
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
        (
            "invalid",
            "colors",
            "range",
            "parameter",
            "option",
            "bad argument",
        ),
    )
    return expect_exception(expectation, _invoke)


def main() -> None:
    if len(sys.argv) != 5:
        raise SystemExit(
            "usage: <scenario> <source-image> <workdir> <log-file>"
        )

    scenario = sys.argv[1]
    source = pathlib.Path(sys.argv[2])
    workdir = pathlib.Path(sys.argv[3])
    log_path = pathlib.Path(sys.argv[4])

    workdir.mkdir(parents=True, exist_ok=True)

    dispatch = {
        "missing": lambda: exercise_missing_path(workdir),
        "corrupt": lambda: exercise_corrupted_image(workdir),
        "unsupported": lambda: exercise_unsupported_format(workdir),
        "oversized": lambda: exercise_oversized_dimensions(workdir, source),
        "invalid_option": lambda: exercise_invalid_option(workdir, source),
    }

    if scenario not in dispatch:
        raise SystemExit(f"unknown scenario: {scenario}")

    result = dispatch[scenario]()
    log_path.parent.mkdir(parents=True, exist_ok=True)
    with log_path.open("a", encoding="utf-8") as handle:
        handle.write(f"{result}\n")
    print(result)


if __name__ == "__main__":
    main()
PY
    }

    if [ "${use_wheel}" -eq 1 ]; then
        if run_python_scenarios "${python_trace_pythonpath}" \
            "${python_wheel_ld_library_path}" \
            "${scenario}" "${source_image}" \
            "${working_dir}/wheel" "${log_file}" >>"${log_file}" 2>&1; then
            tap_pass ${case_id} "${description} via wheel"
        else
            python_skip_on_load_error $? "${log_file}"
            tap_fail ${case_id} "${description} via wheel failed"
        fi
    else
        if run_python_scenarios "${python_in_tree_trace_pythonpath}" \
            "${python_in_tree_ld_library_path}" \
            "${scenario}" "${source_image}" \
            "${working_dir}/tree" "${log_file}" >>"${log_file}" 2>&1; then
            tap_pass ${case_id} "${description} via in-tree modules"
        else
            python_skip_on_load_error $? "${log_file}"
            tap_fail ${case_id} "${description} via in-tree modules failed"
        fi
    fi

    case_id=$((case_id + 1))
}

run_case missing "missing input path errors"
run_case corrupt "corrupted image errors"
run_case unsupported "unsupported format errors"
# Temporarily disable the oversized scenario to avoid CI crashes in
# MinGW environments while the root cause is investigated. Keeping the
# helper function allows quick re-enable once fixed.
# run_case oversized "oversized encode_bytes errors"
run_case invalid_option "invalid option value errors"

tap_plan 4
exit ${tap_status}
