#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0083_mpet_bmp_absolute || {
    echo "not ok 1 - builtin BMP derives absolute mpet samples using the media white once"
    exit 0
}

echo "ok 1 - builtin BMP derives absolute mpet samples using the media white once"
exit 0
