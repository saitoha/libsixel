#!/bin/sh
# TAP wrapper for gd pixelformat case: indexed keycolor reqcolors boundary.

set -eux

test "${HAVE_DECL_GDIMAGECREATEFROMPNGPTR-}" = 1 || {
    printf "1..0 # SKIP HAVE_DECL_GDIMAGECREATEFROMPNGPTR is unavailable\n"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0011_loader_gd_pixelformat" \
    "indexed_keycolor_reqcolors_boundary_pal8" || {
    printf '\n%s\n' "not ok 1 - gd indexed_keycolor_reqcolors_boundary_pal8"
    exit 0
}

printf '\n%s\n' "ok 1 - gd indexed_keycolor_reqcolors_boundary_pal8"
exit 0
