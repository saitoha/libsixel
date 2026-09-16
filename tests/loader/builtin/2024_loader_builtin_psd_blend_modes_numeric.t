#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/psd.md
# Verify all 27 recognized PSD blend keys with exact pixels.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0158_loader_builtin_psd_blend_modes_numeric" 1>&2 || {
    echo "not ok" 1 - "builtin PSD blend modes produce exact pixels"
    exit 0
}

echo "ok" 1 - "builtin PSD blend modes produce exact pixels"
exit 0
