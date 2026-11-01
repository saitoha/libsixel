#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Rebind to a new source and switch from GPU to CPU without stale data.
set -eux
echo "1..1"
set -v
set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0177_gpu_public_rebind_transition"
result=$?
set -e
test "$result" != 77 || {
    echo "ok 1 - gpu public rebind transition # SKIP Metal device unavailable"
    exit 0
}
test "$result" = 0 || {
    echo "not ok 1 - gpu public rebind transition"
    exit 0
}
echo "ok 1 - gpu public rebind transition"
exit 0
