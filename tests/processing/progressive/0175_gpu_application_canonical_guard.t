#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Reject equal canonical error even when GPU float distance rounds down.
set -eux
echo "1..1"
set -v
set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0175_gpu_application_canonical_guard"
result=$?
set -e
test "$result" != 77 || {
    echo "ok 1 - gpu application canonical guard # SKIP Metal device unavailable"
    exit 0
}
test "$result" = 0 || {
    echo "not ok 1 - gpu application canonical guard"
    exit 0
}
echo "ok 1 - gpu application canonical guard"
exit 0
