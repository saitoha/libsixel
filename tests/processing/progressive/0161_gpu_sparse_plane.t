#!/bin/sh
# Verify gpu sparse plane for the private progressive GPU boundary.
set -eux
echo "1..1"
set -v
set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0161_gpu_sparse_plane"
result=$?
set -e
test "$result" != 77 || {
    echo "ok 1 - gpu sparse plane # SKIP Metal device unavailable"
    exit 0
}
test "$result" = 0 || {
    echo "not ok 1 - gpu sparse plane"
    exit 0
}
echo "ok 1 - gpu sparse plane"
exit 0
