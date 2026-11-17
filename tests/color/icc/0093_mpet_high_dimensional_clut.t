#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0093_mpet_high_dimensional_clut || {
    echo "not ok 1 - A sixteen-axis mpet CLUT uses bounded simplex interpolation"
    exit 0
}

echo "ok 1 - A sixteen-axis mpet CLUT uses bounded simplex interpolation"
exit 0
