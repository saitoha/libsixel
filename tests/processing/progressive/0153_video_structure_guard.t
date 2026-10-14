#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Verify video structure guard.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0153_video_structure_guard" || {
    echo "not ok 1 - video structure guard"
    exit 0
}
echo "ok 1 - video structure guard"
exit 0
