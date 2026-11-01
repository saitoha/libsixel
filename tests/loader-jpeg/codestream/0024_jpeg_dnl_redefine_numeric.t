#!/bin/sh
# Policy: docs/loader/builtin/jpeg.md
# Test-plan: docs/testing/builtin-loader-coverage.md
# Use DNL to redefine a known SOF height.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0223_jpeg_dnl_redefine_numeric" 1>&2 || {
    echo "not ok 1 - Use DNL to redefine a known SOF height"
    exit 0
}
echo "ok 1 - Use DNL to redefine a known SOF height"
exit 0
