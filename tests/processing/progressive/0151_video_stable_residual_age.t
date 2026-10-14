#!/bin/sh
# Policy: docs/encoder/output/delta-encoding.md
# Verify video stable residual age.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0151_video_stable_residual_age" || {
    echo "not ok 1 - video stable residual age"
    exit 0
}
echo "ok 1 - video stable residual age"
exit 0
