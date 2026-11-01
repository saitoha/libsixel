#!/bin/sh
# Verify projection hls.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0041_projection_hls" || {
    echo "not ok 1 - projection hls"
    exit 0
}
echo "ok 1 - projection hls"
exit 0
