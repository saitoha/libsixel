#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0055_mpet_float_clut || {
    echo "not ok 1 - mpet CLUT interpolates float samples and clips only input"
    exit 0
}

echo "ok 1 - mpet CLUT interpolates float samples and clips only input"
exit 0
