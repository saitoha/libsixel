#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0084_mpet_bmp_malformed_recovery || {
    echo "not ok 1 - builtin BMP rejects malformed mpet and can load a valid profile next"
    exit 0
}

echo "ok 1 - builtin BMP rejects malformed mpet and can load a valid profile next"
exit 0
