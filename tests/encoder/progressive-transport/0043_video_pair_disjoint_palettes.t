#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Verify video pair disjoint palettes.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0150_video_pair_disjoint_palettes" || {
    printf '\n%s\n' "not ok 1 - video pair disjoint palettes"
    exit 0
}
printf '\n%s\n' "ok 1 - video pair disjoint palettes"
exit 0
