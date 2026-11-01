#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Verify step admitted packet order.
set -eux

echo "1..1"
set -v

set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0094_step_admitted_packet_order"
result=$?
set -e
test "$result" -ne 77 || {
    echo "ok 1 - step admitted packet order # SKIP thread support disabled"
    exit 0
}
test "$result" -eq 0 || {
    echo "not ok 1 - step admitted packet order"
    exit 0
}
echo "ok 1 - step admitted packet order"
exit 0
