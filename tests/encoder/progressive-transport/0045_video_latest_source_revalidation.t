#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Verify video latest source revalidation.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0152_video_latest_source_revalidation" || {
    echo "not ok 1 - video latest source revalidation"
    exit 0
}
echo "ok 1 - video latest source revalidation"
exit 0
