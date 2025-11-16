#!/usr/bin/env bash
# Verify error handling for invalid img2sixel options.
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
# shellcheck source=converters/t/common.t
source "${SCRIPT_DIR}/common.t"

TRACE_INVALID_OPTION=${TRACE_INVALID_OPTION:-}
# Optional tracing hook that records how long each img2sixel invocation takes
# when TRACE_INVALID_OPTION is non-empty.  Windows runners pay a heavy process
# startup cost, so tracking the number of sequential executions helps explain
# why this test frequently exceeds the default 30-second Meson timeout.
TRACE_CLOCK_IMPL=""
INVALID_OPTION_INVOCATIONS=0
INVALID_OPTION_TOTAL_MS=0

timestamp_ms() {
    case ${TRACE_CLOCK_IMPL} in
        python3)
            python3 - <<'EOF'
import time
print(int(time.time() * 1000))
EOF
            ;;
        python)
            python - <<'EOF'
import time
print(int(time.time() * 1000))
EOF
            ;;
        perl)
            perl -MTime::HiRes=time -e 'printf("%d\n", (int)(time() * 1000))'
            ;;
        *)
            date +%s000
            ;;
    esac
}

format_milliseconds() {
    local millis
    local seconds
    local fraction

    millis=$1
    seconds=$((millis / 1000))
    fraction=$((millis % 1000))

    printf '%d.%03d' "${seconds}" "${fraction}"
}

trace_invalid_option() {
    local elapsed
    local label

    if [[ -z ${TRACE_INVALID_OPTION} ]]; then
        return
    fi

    elapsed=$(format_milliseconds "$1")
    label=$2

    printf '[trace][invalid-option] %s (elapsed %ss)\n' \
        "${label}" "${elapsed}" >&2
}

trace_summary() {
    local total

    if [[ -z ${TRACE_INVALID_OPTION} ]]; then
        return
    fi

    total=$(format_milliseconds "${INVALID_OPTION_TOTAL_MS}")
    printf '[trace][invalid-option] %u img2sixel invocations, %ss total\n' \
        "${INVALID_OPTION_INVOCATIONS}" "${total}" >&2
}

if [[ -n ${TRACE_INVALID_OPTION} ]]; then
    if command -v python3 >/dev/null 2>&1; then
        TRACE_CLOCK_IMPL="python3"
    elif command -v python >/dev/null 2>&1; then
        TRACE_CLOCK_IMPL="python"
    elif command -v perl >/dev/null 2>&1; then
        TRACE_CLOCK_IMPL="perl"
    else
        echo "TRACE_INVALID_OPTION requires python or perl" >&2
        TRACE_INVALID_OPTION=""
    fi

    if [[ -n ${TRACE_INVALID_OPTION} ]]; then
        trap trace_summary EXIT
    fi
fi

echo '[test1] invalid option handling'

# Ensure an unreadable input file does not leave stray output.
invalid_file="${TMP_DIR}/testfile"
rm -f "${invalid_file}"
touch "${invalid_file}"
chmod a-r "${invalid_file}"
# Expect img2sixel to fail cleanly when the source is unreadable.
output_file="${TMP_DIR}/capture.$$"
if run_img2sixel "${invalid_file}" </dev/null >"${output_file}" 2>/dev/null; then
    :
fi
if [[ -s ${output_file} ]]; then
    echo 'img2sixel unexpectedly produced output for unreadable file' >&2
    rm -f "${output_file}"
    exit 1
fi
rm -f "${output_file}"
rm -f "${invalid_file}"

rm -f "${TMP_DIR}/invalid_filename"

expect_failure() {
    local output_file
    local start_ms
    local end_ms
    local elapsed_ms
    local label

    output_file="${TMP_DIR}/capture.$$"
    label="img2sixel $*"
    if [[ -n ${TRACE_INVALID_OPTION} ]]; then
        start_ms=$(timestamp_ms)
    fi
    # Windows builds may fall back to stdin when the input path is rejected.
    # Redirect /dev/null so native executables cannot block on console input
    # if they attempt to read from stdin after the path validation fails.
    if run_img2sixel "$@" </dev/null >"${output_file}" 2>/dev/null; then
        :
    fi
    if [[ -s ${output_file} ]]; then
        printf 'img2sixel unexpectedly produced output: %s\n' "$*" >&2
        rm -f "${output_file}"
        exit 1
    fi
    rm -f "${output_file}"

    if [[ -n ${TRACE_INVALID_OPTION} ]]; then
        end_ms=$(timestamp_ms)
        elapsed_ms=$((end_ms - start_ms))
        INVALID_OPTION_INVOCATIONS=$((INVALID_OPTION_INVOCATIONS + 1))
        INVALID_OPTION_TOTAL_MS=$((INVALID_OPTION_TOTAL_MS + elapsed_ms))
        trace_invalid_option "${elapsed_ms}" "${label}"
    fi
}

# Reject a missing input path.
expect_failure "${TMP_DIR}/invalid_filename"
# Reject a directory as input.
expect_failure "."
# Report an unknown dither option.
expect_failure -d invalid_option
# Report an unknown resize filter.
expect_failure -r invalid_option
# Report an unknown scaling mode.
expect_failure -s invalid_option
# Report an unknown tone adjustment mode.
expect_failure -t invalid_option
# Report an invalid width value.
expect_failure -w invalid_option
# Report an invalid height value.
expect_failure -h invalid_option
# Report an invalid format name.
expect_failure -f invalid_option
# Report an invalid quality preset.
expect_failure -q invalid_option
# Report an invalid layout option.
expect_failure -l invalid_option
# Report an invalid bits-per-pixel argument.
expect_failure -b invalid_option
# Report an invalid encoder tweak.
expect_failure -E invalid_option
# Report an invalid background colour string.
expect_failure -B invalid_option
# Reject a background colour missing one component.
expect_failure -B '#ffff' "${TOP_SRCDIR}/images/map8.png"
# Reject an overly long background colour specification.
expect_failure -B '#0000000000000' "${TOP_SRCDIR}/images/map8.png"
# Reject a malformed hex colour.
expect_failure -B '#00G'
# Reject an unknown named colour.
expect_failure -B test
# Reject an incomplete rgb: colour form.
expect_failure -B 'rgb:11/11'
# Reject the unsupported legacy width syntax.
expect_failure '-%'
# Reject a palette file that does not exist.
expect_failure -m "${TMP_DIR}/invalid_filename" "${TOP_SRCDIR}/images/snake.jpg"
# Reject mutually exclusive palette and encode flags.
expect_failure -p16 -e "${TOP_SRCDIR}/images/snake.jpg"
# Reject an invalid colour space index.
expect_failure -I -C0 "${TOP_SRCDIR}/images/snake.png"
# Reject incompatible inspect and palette options.
expect_failure -I -p8 "${TOP_SRCDIR}/images/snake.png"
# Reject conflicting palette size and terminal preset.
expect_failure -p64 -bxterm256 "${TOP_SRCDIR}/images/snake.png"
# Reject 8-bit output when palette dump is requested.
expect_failure -8 -P "${TOP_SRCDIR}/images/snake.png"
