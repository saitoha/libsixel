#!/bin/sh
# TAP wrapper that dispatches to the unified C test runner.

set -eux

test "${HAVE_WEBP-}" = 1 || {
    printf "1..0 # SKIP libwebp loader is unavailable\n"
    exit 0
}


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "loader/0019_loader_libwebp_palette_promotion_guard" || {
    printf '\n%s\n' "not ok 1 - loader/0019_loader_libwebp_palette_promotion_guard"
    exit 0
}

printf '\n%s\n' "ok 1 - loader/0019_loader_libwebp_palette_promotion_guard"
exit 0
