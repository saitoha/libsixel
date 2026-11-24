#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/pic.md
# Verify chained PIC packets combine exact component lanes.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0102_loader_builtin_pic_chained_packets_numeric" || {
    printf '\n%s\n' "not ok 1 - PIC chained packet exact component lanes"
    exit 0
}

printf '\n%s\n' "ok 1 - PIC chained packet exact component lanes"
exit 0
