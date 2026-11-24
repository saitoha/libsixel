#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Verify video pair write failure.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0155_video_pair_write_failure" || {
    printf '\n%s\n' "not ok 1 - video pair write failure"
    exit 0
}
printf '\n%s\n' "ok 1 - video pair write failure"
exit 0
