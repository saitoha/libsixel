#!/bin/sh
# TAP wrapper for gd pixelformat case: highdepth float32.

set -eux

test "${HAVE_DECL_GDIMAGECREATEFROMPNGPTR-}" = 1 || {
    printf "1..0 # SKIP HAVE_DECL_GDIMAGECREATEFROMPNGPTR is unavailable\n"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0011_loader_gd_pixelformat" "highdepth_float32" || {
    printf '\n%s\n' "not ok 1 - gd highdepth_float32"
    exit 0
}

printf '\n%s\n' "ok 1 - gd highdepth_float32"
exit 0
