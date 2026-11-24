#!/bin/sh
# TAP wrapper for gd pixelpolicy case: single nonzero key is preserved.

set -eux

test "${HAVE_DECL_GDIMAGECREATEFROMPNGPTR-}" = 1 || {
    printf "1..0 # SKIP HAVE_DECL_GDIMAGECREATEFROMPNGPTR is unavailable\n"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0059_loader_gd_pixelpolicy_detail" \
    "single_key_nonzero_preserved" || {
    printf '\n%s\n' "not ok 1 - gd single_key_nonzero_preserved"
    exit 0
}

printf '\n%s\n' "ok 1 - gd single_key_nonzero_preserved"
exit 0
