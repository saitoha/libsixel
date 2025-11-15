#!/usr/bin/env bash
# Exercise regressions tracked under issue numbers.
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
# shellcheck source=converters/t/common.t
source "${SCRIPT_DIR}/common.t"

echo '[test13] regression test'

require_file "${TOP_SRCDIR}/tests/issue/167/poc"
require_file "${TOP_SRCDIR}/tests/issue/166/poc"
require_file "${TOP_SRCDIR}/tests/issue/200/POC_img2sixel_heap_buffer_overflow"

check_exit() {
    if run_img2sixel "$@"; then
        rc=0
    else
        rc=$?
    fi
    case ${rc} in
        0|127|255)
            ;;
        *)
            printf 'Unexpected exit code %d for img2sixel %s\n' "${rc}" "$*" >&2
            exit 1
            ;;
    esac
}

# Regression 167: tolerate empty background strings.
check_exit -B '#000' -B ''
# Regression 167: handle crafted input with specific height.
check_exit "${TOP_SRCDIR}/tests/issue/167/poc" -h128
# Regression 166: handle crafted input with specific width.
check_exit "${TOP_SRCDIR}/tests/issue/166/poc" -w128
# Regression 200: ensure complex flag mix avoids heap overflow.
run_img2sixel --7bit-mode -8 --invert --palette-type=auto --verbose \
    "${TOP_SRCDIR}/tests/issue/200/POC_img2sixel_heap_buffer_overflow" -o /dev/null
