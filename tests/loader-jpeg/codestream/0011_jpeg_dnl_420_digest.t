#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/jpeg.md
# Preserve the fixed progressive 4:2:0 pixel digest with DNL.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0210_jpeg_dnl_420_digest" 1>&2 || {
    echo "not ok 1 - Preserve the fixed progressive 4:2:0 pixel digest with DNL"
    exit 0
}
echo "ok 1 - Preserve the fixed progressive 4:2:0 pixel digest with DNL"
exit 0
