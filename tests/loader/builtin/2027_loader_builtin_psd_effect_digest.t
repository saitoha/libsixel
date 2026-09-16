#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/psd.md
# Verify representative PSD layer effects with a complete RGB digest.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0161_loader_builtin_psd_effect_digest" 1>&2 || {
    echo "not ok" 1 - "builtin PSD layer-effect output matches its digest"
    exit 0
}

echo "ok" 1 - "builtin PSD layer-effect output matches its digest"
exit 0
