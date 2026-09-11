#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/pic.md
# Verify PIC ten-packet upper bound with a minimal in-memory stream.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0093_loader_builtin_pic_ten_packet_limit_numeric" || {
    echo "not ok 1 - PIC ten-packet upper bound"
    exit 0
}

echo "ok 1 - PIC ten-packet upper bound"
exit 0
