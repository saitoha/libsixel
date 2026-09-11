#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/pic.md
# Verify the descriptor-table upper bound rejects packet eleven.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0121_loader_builtin_pic_eleventh_packet_reject" || {
    echo "not ok 1 - PIC eleventh packet is rejected"
    exit 0
}

echo "ok 1 - PIC eleventh packet is rejected"
exit 0
