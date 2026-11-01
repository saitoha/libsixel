#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Reject DNL before the first scan.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0215_jpeg_dnl_prescan_reject" 1>&2 || {
    echo "not ok 1 - Reject DNL before the first scan"
    exit 0
}
echo "ok 1 - Reject DNL before the first scan"
exit 0
