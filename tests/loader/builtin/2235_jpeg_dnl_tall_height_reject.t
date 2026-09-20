#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Reject a DNL height requiring missing encoded blocks.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0219_jpeg_dnl_tall_height_reject" 1>&2 || {
    echo "not ok 1 - Reject a DNL height requiring missing encoded blocks"
    exit 0
}
echo "ok 1 - Reject a DNL height requiring missing encoded blocks"
exit 0
