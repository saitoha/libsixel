#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/jpeg.md
# Continue progressive AC scans after the first-scan DNL.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0206_jpeg_dnl_progressive_numeric" 1>&2 || {
    echo "not ok 1 - Continue progressive AC scans after the first-scan DNL"
    exit 0
}
echo "ok 1 - Continue progressive AC scans after the first-scan DNL"
exit 0
