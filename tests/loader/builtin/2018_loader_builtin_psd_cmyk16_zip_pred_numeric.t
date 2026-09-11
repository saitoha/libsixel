#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/psd.md
# Verify PSD CMYK16 ZIP prediction keeps exact samples.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0152_loader_builtin_psd_cmyk16_zip_pred_numeric" 1>&2 || {
    echo "not ok" 1 - "PSD CMYK16 ZIP prediction keeps exact samples"
    exit 0
}

echo "ok" 1 - "PSD CMYK16 ZIP prediction keeps exact samples"

exit 0
