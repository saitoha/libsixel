#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/psd.md
# Verify PSD clipping reconstruction with a complete RGB digest.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0159_loader_builtin_psd_clipping_digest" 1>&2 || {
    echo "not ok" 1 - "builtin PSD clipping output matches its digest"
    exit 0
}

echo "ok" 1 - "builtin PSD clipping output matches its digest"
exit 0
