#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Finish an admitted GPU repair before a waiting context destruction.
set -eux
echo "1..1"
set -v
set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0197_gpu_pending_destroy"
result=$?
set -e
test "$result" != 77 || {
    echo "ok 1 - gpu pending destroy # SKIP threads or Metal unavailable"
    exit 0
}
test "$result" = 0 || {
    echo "not ok 1 - gpu pending destroy"
    exit 0
}
echo "ok 1 - gpu pending destroy"
exit 0
