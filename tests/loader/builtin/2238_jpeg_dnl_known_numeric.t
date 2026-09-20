#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/jpeg.md
# Preserve known-height JPEG with a matching DNL.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0222_jpeg_dnl_known_numeric" 1>&2 || {
    echo "not ok 1 - Preserve known-height JPEG with a matching DNL"
    exit 0
}
echo "ok 1 - Preserve known-height JPEG with a matching DNL"
exit 0
