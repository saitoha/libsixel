#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0096_mpet_lab_neutral_reverse || {
    echo "not ok 1 - mpet retains small signed Lab chroma in the reverse direction"
    exit 0
}

echo "ok 1 - mpet retains small signed Lab chroma in the reverse direction"
exit 0
