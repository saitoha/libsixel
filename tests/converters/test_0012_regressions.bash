#!/usr/bin/env bash
# Exercise regressions tracked under issue numbers.
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
# shellcheck source=tests/converters/common.bash
source "${SCRIPT_DIR}/common.bash"

# ----------------------------------------------------------------------
#  +----------------------+------------------------------------------+
#  | Issue                | Behaviour to verify                      |
#  +----------------------+------------------------------------------+
#  | #167                 | Empty background string, crafted input   |
#  | #166                 | Crafted width handling                   |
#  | #200                 | Complex flag mix without overflow        |
#  +----------------------+------------------------------------------+
# ----------------------------------------------------------------------

tap_init "$(basename "$0")"

require_file "${TOP_SRCDIR}/tests/issue/167/poc"
require_file "${TOP_SRCDIR}/tests/issue/166/poc"
require_file "${TOP_SRCDIR}/tests/issue/200/POC_img2sixel_heap_buffer_overflow"

allowed_exit_case() {
    local description
    local command
    local rc

    description=$1
    shift
    command=("$@")
    tap_log "[regression] ${description} :: ${command[*]}"
    if "${command[@]}"; then
        rc=0
    else
        rc=$?
    fi
    case ${rc} in
        0|127|255)
            return 0
            ;;
        *)
            printf 'Unexpected exit code %d for img2sixel %s\n' \
                "${rc}" "${command[*]}"
            return 1
            ;;
    esac
}

regression_200_case() {
    tap_log '[regression] issue 200 complex flags'
    run_img2sixel --7bit-mode -8 --invert --palette-type=auto --verbose \
        "${TOP_SRCDIR}/tests/issue/200/POC_img2sixel_heap_buffer_overflow" \
        -o /dev/null
}

tap_plan 4

tap_case 'issue 167 empty background tolerated' allowed_exit_case \
    'issue 167 background' run_img2sixel -B '#000' -B ''
tap_case 'issue 167 crafted height file' allowed_exit_case \
    'issue 167 poc' run_img2sixel "${TOP_SRCDIR}/tests/issue/167/poc" -h128
tap_case 'issue 166 crafted width file' allowed_exit_case \
    'issue 166 poc' run_img2sixel "${TOP_SRCDIR}/tests/issue/166/poc" -w128
tap_case 'issue 200 complex flag mix' regression_200_case
