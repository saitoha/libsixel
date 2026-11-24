#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Verify video structure guard.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0153_video_structure_guard" || {
    printf '\n%s\n' "not ok 1 - video structure guard"
    exit 0
}
printf '\n%s\n' "ok 1 - video structure guard"
exit 0
