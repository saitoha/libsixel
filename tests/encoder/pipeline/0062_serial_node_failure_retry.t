#!/bin/sh
# Verify serial node failure retry.
# Policy: docs/threading/encoder.md
set -eux

echo "1..1"
set -v

set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/pipeline/0062_serial_node_failure_retry"
status=$?
set -e
test "$status" -ne 77 || {
    echo "ok 1 - serial node failure retry # SKIP threads unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - serial node failure retry"
    exit 0
}
echo "ok 1 - serial node failure retry"
exit 0
