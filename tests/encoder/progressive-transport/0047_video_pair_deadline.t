#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Verify video pair deadline.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0154_video_pair_deadline" || {
    echo "not ok 1 - video pair deadline"
    exit 0
}
echo "ok 1 - video pair deadline"
exit 0
