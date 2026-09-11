#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/png.md
# Policy: docs/loader/builtin-cms.md
# Verify cHRM/gAMA matrix conversion at float frame precision.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0174_loader_builtin_png_chrm_gama_float_numeric" 1>&2 || {
    echo "not ok" 1 - "PNG cHRM/gAMA float samples"
    exit 0
}
echo "ok" 1 - "PNG cHRM/gAMA float samples"
exit 0
