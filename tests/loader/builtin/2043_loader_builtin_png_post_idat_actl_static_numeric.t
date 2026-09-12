#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/png.md
# Verify acTL after the first IDAT does not classify a static PNG as APNG.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0177_loader_builtin_png_post_idat_actl_static_numeric" 1>&2 || {
    echo "not ok" 1 - "post-IDAT acTL selected APNG"
    exit 0
}
echo "ok" 1 - "post-IDAT acTL remains static PNG"
exit 0
