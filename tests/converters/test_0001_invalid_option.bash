#!/usr/bin/env bash
# Verify error handling for invalid img2sixel options.
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
# shellcheck source=tests/t/common.bash
source "${SCRIPT_DIR}/common.bash"

# ----------------------------------------------------------------------
#  +---------------------------------------------------------------+
#  | Test layout                                                   |
#  +---------------------------------------------------------------+
#  | Case 1  | Unreadable input file should not yield an output.   |
#  | Case 2+ | Each suspicious option must be rejected cleanly.    |
#  +---------------------------------------------------------------+
#  The ASCII table makes it easy to align the TAP plan with the
#  registered cases.
# ----------------------------------------------------------------------

tap_init "$(basename "$0")"

declare -a EXPECT_FAILURE_DESCRIPTIONS=()
declare -a EXPECT_FAILURE_ARGUMENTS=()

register_expect_failure() {
    local description

    description=$1
    shift
    EXPECT_FAILURE_DESCRIPTIONS+=("${description}")
    EXPECT_FAILURE_ARGUMENTS+=("$*")
}

expect_failure_case() {
    local output_file

    output_file="${TMP_DIR}/capture.$$"
    tap_log "[invalid-option] checking: $*"
    # Redirect stdin from /dev/null so tests like `img2sixel -` fail
    # immediately instead of waiting for input.  This keeps the TAP
    # execution from hanging.
    if run_img2sixel "$@" < /dev/null >"${output_file}" 2>/dev/null; then
        :
    fi
    if [[ -s ${output_file} ]]; then
        printf 'img2sixel unexpectedly produced output: %s\n' "$*"
        rm -f "${output_file}"
        return 1
    fi
    rm -f "${output_file}"
    return 0
}

prepare_unreadable_case() {
    local invalid_file
    local output_file

    tap_log "[invalid-option] unreadable file guard"
    invalid_file="${TMP_DIR}/testfile"
    rm -f "${invalid_file}"
    touch "${invalid_file}"
    chmod a-r "${invalid_file}"
    output_file="${TMP_DIR}/capture.$$"
    if run_img2sixel "${invalid_file}" >"${output_file}" 2>/dev/null; then
        :
    fi
    if [[ -s ${output_file} ]]; then
        printf 'img2sixel unexpectedly produced output for unreadable file\n'
        rm -f "${output_file}"
        rm -f "${invalid_file}"
        return 1
    fi
    rm -f "${output_file}"
    rm -f "${invalid_file}"
    rm -f "${TMP_DIR}/invalid_filename"
    return 0
}

register_expect_failure "missing input path" "${TMP_DIR}/invalid_filename"
register_expect_failure "current directory as input" .
register_expect_failure "invalid -d argument" -d invalid_option
register_expect_failure "invalid -r argument" -r invalid_option
register_expect_failure "invalid -s argument" -s invalid_option
register_expect_failure "invalid -t argument" -t invalid_option
register_expect_failure "invalid -w argument" -w invalid_option
register_expect_failure "invalid -h argument" -h invalid_option
register_expect_failure "invalid -f argument" -f invalid_option
register_expect_failure "invalid -q argument" -q invalid_option
register_expect_failure "invalid -l argument" -l invalid_option
register_expect_failure "invalid -b argument" -b invalid_option
register_expect_failure "invalid -E argument" -E invalid_option
register_expect_failure "invalid -B argument" -B invalid_option
register_expect_failure "reject malformed -B '#ffff'" -B "#ffff" "${TOP_SRCDIR}/images/map8.png"
register_expect_failure "reject malformed -B long" -B "#0000000000000" "${TOP_SRCDIR}/images/map8.png"
register_expect_failure "reject malformed -B '#00G'" -B "#00G"
register_expect_failure "reject malformed -B name" -B test
register_expect_failure "reject malformed -B rgb pair" -B "rgb:11/11"
register_expect_failure "reject bare hyphen" -
register_expect_failure "reject stray percent" -%
register_expect_failure "reject -m with invalid file" -m "${TMP_DIR}/invalid_filename" "${TOP_SRCDIR}/images/snake.jpg"
register_expect_failure "reject -p with invalid file" -p16 -e "${TOP_SRCDIR}/images/snake.jpg"
register_expect_failure "reject -I -C0" -I -C0 "${TOP_SRCDIR}/images/snake.png"
register_expect_failure "reject -I -p8" -I -p8 "${TOP_SRCDIR}/images/snake.png"
register_expect_failure "reject -p64 -bxterm256" -p64 -bxterm256 "${TOP_SRCDIR}/images/snake.png"
register_expect_failure "reject -8 -P" -8 -P "${TOP_SRCDIR}/images/snake.png"

case_count=$((1 + ${#EXPECT_FAILURE_DESCRIPTIONS[@]}))
tap_plan "${case_count}"

tap_case 'unreadable input rejects output' prepare_unreadable_case

for index in "${!EXPECT_FAILURE_DESCRIPTIONS[@]}"; do
    description=${EXPECT_FAILURE_DESCRIPTIONS[${index}]}
    args_string=${EXPECT_FAILURE_ARGUMENTS[${index}]}
    IFS=$' \t\n' read -r -a args <<<"${args_string}"
    tap_case "${description}" expect_failure_case "${args[@]}"
done
