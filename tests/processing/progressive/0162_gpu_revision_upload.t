#!/bin/sh
# Verify gpu revision upload for the private progressive GPU boundary.
set -eux
echo "1..1"
set -v
set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0162_gpu_revision_upload"
result=$?
set -e
test "$result" != 77 || {
    echo "ok 1 - gpu revision upload # SKIP Metal device unavailable"
    exit 0
}
test "$result" = 0 || {
    echo "not ok 1 - gpu revision upload"
    exit 0
}
echo "ok 1 - gpu revision upload"
exit 0
