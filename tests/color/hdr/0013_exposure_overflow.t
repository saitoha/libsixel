#!/bin/sh
# Regression: exposure overflow.
set -eux
echo '1..1'
set -v
SIXEL_LOADER_HDR_TONEMAP=auto SIXEL_LOADER_HDR_EXPOSURE_EV=1024 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" hdr/0013_exposure_overflow || {
    echo 'not ok 1 - exposure overflow'
    exit 0
}
echo 'ok 1 - exposure overflow'
exit 0
