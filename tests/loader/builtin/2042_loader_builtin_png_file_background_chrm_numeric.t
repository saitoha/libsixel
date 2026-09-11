#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/png.md
# Policy: docs/loader/background-policy.md
# Verify PNG bKGD uses the same cHRM/gAMA source interpretation.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0176_loader_builtin_png_file_background_chrm_numeric" 1>&2 || {
    echo "not ok" 1 - "PNG cHRM/gAMA file-background samples"
    exit 0
}
echo "ok" 1 - "PNG cHRM/gAMA file-background samples"
exit 0
