#!/bin/sh
# Verify projection hls.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0041_projection_hls" || {
    printf '\n%s\n' "not ok 1 - projection hls"
    exit 0
}
printf '\n%s\n' "ok 1 - projection hls"
exit 0
