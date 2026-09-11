#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/psd.md
# Fix decoded float samples from PSD 16-bit ZIP prediction.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0116_loader_builtin_psd_rgb16_zip_pred_numeric" || {
    echo "not ok 1 - PSD RGB16 ZIP prediction numeric samples"
    exit 0
}

echo "ok 1 - PSD RGB16 ZIP prediction numeric samples"
exit 0
