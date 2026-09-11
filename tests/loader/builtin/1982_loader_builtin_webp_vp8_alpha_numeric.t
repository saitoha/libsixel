#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/webp.md
# Fix VP8+ALPH RGB bytes and the separate transparency mask.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0114_loader_builtin_webp_vp8_alpha_numeric" || {
    echo "not ok 1 - WebP VP8+ALPH exact pixels and mask"
    exit 0
}

echo "ok 1 - WebP VP8+ALPH exact pixels and mask"
exit 0
