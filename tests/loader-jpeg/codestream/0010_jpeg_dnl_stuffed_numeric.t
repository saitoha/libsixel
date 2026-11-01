#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/jpeg.md
# Skip stuffed FF bytes while finding the first-scan DNL.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0209_jpeg_dnl_stuffed_numeric" 1>&2 || {
    echo "not ok 1 - Skip stuffed FF bytes while finding the first-scan DNL"
    exit 0
}
echo "ok 1 - Skip stuffed FF bytes while finding the first-scan DNL"
exit 0
