#!/bin/sh
# Verify parallel node drain release.
# Policy: docs/threading/encoder.md
set -eux

echo "1..1"
set -v

set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/pipeline/0058_parallel_node_drain_release"
status=$?
set -e
test "$status" -ne 77 || {
    echo "ok 1 - parallel node drain release # SKIP threads unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - parallel node drain release"
    exit 0
}
echo "ok 1 - parallel node drain release"
exit 0
