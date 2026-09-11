#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/png.md
# Policy: docs/loader/builtin-cms.md
# Verify singular cHRM falls back to the usable gAMA path.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0175_loader_builtin_png_singular_chrm_fallback_numeric" 1>&2 || {
    echo "not ok" 1 - "PNG singular cHRM gamma fallback"
    exit 0
}
echo "ok" 1 - "PNG singular cHRM gamma fallback"
exit 0
