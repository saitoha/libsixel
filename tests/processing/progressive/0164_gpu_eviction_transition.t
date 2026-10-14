#!/bin/sh
# Verify gpu eviction transition for the private progressive GPU boundary.
set -eux
echo "1..1"
set -v
set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0164_gpu_eviction_transition"
result=$?
set -e
test "$result" != 77 || {
    echo "ok 1 - gpu eviction transition # SKIP Metal device unavailable"
    exit 0
}
test "$result" = 0 || {
    echo "not ok 1 - gpu eviction transition"
    exit 0
}
echo "ok 1 - gpu eviction transition"
exit 0
