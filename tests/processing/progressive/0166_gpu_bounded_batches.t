#!/bin/sh
# Verify gpu bounded batches for the private progressive GPU boundary.
set -eux
echo "1..1"
set -v
set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0166_gpu_bounded_batches"
result=$?
set -e
test "$result" != 77 || {
    echo "ok 1 - gpu bounded batches # SKIP Metal device unavailable"
    exit 0
}
test "$result" = 0 || {
    echo "not ok 1 - gpu bounded batches"
    exit 0
}
echo "ok 1 - gpu bounded batches"
exit 0
