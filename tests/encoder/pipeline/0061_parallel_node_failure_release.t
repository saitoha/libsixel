#!/bin/sh
# Verify parallel node failure release.
# Policy: docs/threading/encoder.md
set -eux

echo "1..1"
set -v

set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/pipeline/0061_parallel_node_failure_release"
status=$?
set -e
test "$status" -ne 77 || {
    echo "ok 1 - parallel node failure release # SKIP threads unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - parallel node failure release"
    exit 0
}
echo "ok 1 - parallel node failure release"
exit 0
