#!/bin/sh
# TAP wrapper for gd pixelformat case: indexed single trns semi mask.

set -eux

test "${HAVE_DECL_GDIMAGECREATEFROMPNGPTR-}" = 1 || {
    printf "1..0 # SKIP HAVE_DECL_GDIMAGECREATEFROMPNGPTR is unavailable\n"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0011_loader_gd_pixelformat" "indexed_single_trns_semi_mask" || {
    echo "not ok 1 - gd indexed_single_trns_semi_mask"
    exit 0
}

echo "ok 1 - gd indexed_single_trns_semi_mask"
exit 0
