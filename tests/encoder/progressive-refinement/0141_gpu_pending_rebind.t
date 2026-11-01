#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Keep GPU binding storage alive until a waiting rebind owns the gate.
set -eux
echo "1..1"
set -v
set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0196_gpu_pending_rebind"
result=$?
set -e
test "$result" != 77 || {
    echo "ok 1 - gpu pending rebind # SKIP threads or Metal unavailable"
    exit 0
}
test "$result" = 0 || {
    echo "not ok 1 - gpu pending rebind"
    exit 0
}
echo "ok 1 - gpu pending rebind"
exit 0
