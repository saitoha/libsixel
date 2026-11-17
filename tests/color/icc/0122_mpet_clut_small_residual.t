#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0122_mpet_clut_small_residual || {
    echo "not ok 1 - one axis CLUT retains an amplified residual"
    exit 0
}

echo "ok 1 - one axis CLUT retains an amplified residual"
exit 0
