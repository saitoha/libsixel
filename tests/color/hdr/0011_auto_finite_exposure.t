#!/bin/sh
# Regression: auto finite exposure.
set -eux
echo '1..1'
set -v
SIXEL_LOADER_HDR_TONEMAP=auto SIXEL_LOADER_HDR_EXPOSURE_EV=1023 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" hdr/0011_auto_finite_exposure || {
    echo 'not ok 1 - auto finite exposure'
    exit 0
}
echo 'ok 1 - auto finite exposure'
exit 0
