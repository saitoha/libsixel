#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Reject a DNL length below four.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0213_jpeg_dnl_length_reject" 1>&2 || {
    echo "not ok 1 - Reject a DNL length below four"
    exit 0
}
echo "ok 1 - Reject a DNL length below four"
exit 0
