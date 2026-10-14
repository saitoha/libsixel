#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Verify video final full domain.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0156_video_final_full_domain" || {
    echo "not ok 1 - video final full domain"
    exit 0
}
echo "ok 1 - video final full domain"
exit 0
