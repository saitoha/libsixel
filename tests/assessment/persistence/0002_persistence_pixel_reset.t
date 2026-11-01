#!/bin/sh
# Policy: docs/quality/video-evaluation.md
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" assessment/0037_persistence_pixel_reset || {
    echo "not ok 1 - persistence pixel reset"
    exit 0
}
echo "ok 1 - persistence pixel reset"
exit 0
