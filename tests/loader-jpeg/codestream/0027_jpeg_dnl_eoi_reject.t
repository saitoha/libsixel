#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Reject a deferred-height frame truncated after DNL.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0226_jpeg_dnl_eoi_reject" 1>&2 || {
    echo "not ok 1 - Reject a deferred-height frame truncated after DNL"
    exit 0
}
echo "ok 1 - Reject a deferred-height frame truncated after DNL"
exit 0
