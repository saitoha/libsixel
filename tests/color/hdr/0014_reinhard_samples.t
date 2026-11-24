#!/bin/sh
# Regression: reinhard samples.
set -eux
echo '1..1'
set -v
SIXEL_LOADER_HDR_TONEMAP=reinhard SIXEL_LOADER_HDR_EXPOSURE_EV=0 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" hdr/0014_reinhard_samples || {
    printf '\n%s\n' 'not ok 1 - reinhard samples'
    exit 0
}
printf '\n%s\n' 'ok 1 - reinhard samples'
exit 0
