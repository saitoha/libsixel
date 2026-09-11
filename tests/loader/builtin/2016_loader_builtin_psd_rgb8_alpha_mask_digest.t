#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/psd.md
# Verify PSD RGB8 emits exact RGB and alpha mask.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0150_loader_builtin_psd_rgb8_alpha_mask_digest" 1>&2 || {
    echo "not ok" 1 - "PSD RGB8 emits exact RGB and alpha mask"
    exit 0
}

echo "ok" 1 - "PSD RGB8 emits exact RGB and alpha mask"

exit 0
