#!/bin/sh
# TAP wrapper for gd pixelpolicy case: partial alpha mask semantics.

set -eux

test "${HAVE_DECL_GDIMAGECREATEFROMPNGPTR-}" = 1 || {
    printf "1..0 # SKIP HAVE_DECL_GDIMAGECREATEFROMPNGPTR is unavailable\n"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0059_loader_gd_pixelpolicy_detail" \
    "partial_alpha_mask_semantics" || {
    echo "not ok 1 - gd partial_alpha_mask_semantics"
    exit 0
}

echo "ok 1 - gd partial_alpha_mask_semantics"
exit 0
