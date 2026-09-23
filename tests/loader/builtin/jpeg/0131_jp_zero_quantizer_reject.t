#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Reject zero quantizer.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "loader/jpeg/0131_jp_zero_quantizer_reject" 1>&2 || {
    echo "not ok 1 - Reject zero quantizer"
    exit 0
}
echo "ok 1 - Reject zero quantizer"
exit 0
