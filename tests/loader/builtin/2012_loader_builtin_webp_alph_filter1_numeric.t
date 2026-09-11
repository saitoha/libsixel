#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/webp.md
# Verify WebP ALPH filter 1 produces exact RGB and mask.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0146_loader_builtin_webp_alph_filter1_numeric" 1>&2 || {
    echo "not ok" 1 - "WebP ALPH filter 1 produces exact RGB and mask"
    exit 0
}

echo "ok" 1 - "WebP ALPH filter 1 produces exact RGB and mask"

exit 0
