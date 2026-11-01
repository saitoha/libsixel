#!/bin/sh
# Policy: docs/quality/video-evaluation.md
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" assessment/0038_persistence_partition_invariance || {
    echo "not ok 1 - persistence partition invariance"
    exit 0
}
echo "ok 1 - persistence partition invariance"
exit 0
