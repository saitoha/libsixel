#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0095_mpet_lab_neutral_forward || {
    printf '\n%s\n' "not ok 1 - mpet retains small signed Lab chroma in the forward direction"
    exit 0
}

printf '\n%s\n' "ok 1 - mpet retains small signed Lab chroma in the forward direction"
exit 0
