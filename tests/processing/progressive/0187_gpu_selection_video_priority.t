#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Preserve luminance priority masks and weighted sample populations.
set -eux
echo "1..1"
set -v
set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0187_gpu_selection_video_priority"
result=$?
set -e
test "$result" != 77 || {
    echo "ok 1 - gpu selection video priority # SKIP Metal device unavailable"
    exit 0
}
test "$result" = 0 || {
    echo "not ok 1 - gpu selection video priority"
    exit 0
}
echo "ok 1 - gpu selection video priority"
exit 0
