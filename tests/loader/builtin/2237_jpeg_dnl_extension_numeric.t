#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/jpeg.md
# Skip bounded DNL extension bytes without decoding them.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0221_jpeg_dnl_extension_numeric" 1>&2 || {
    echo "not ok 1 - Skip bounded DNL extension bytes without decoding them"
    exit 0
}
echo "ok 1 - Skip bounded DNL extension bytes without decoding them"
exit 0
