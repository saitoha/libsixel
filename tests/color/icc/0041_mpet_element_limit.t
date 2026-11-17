#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0041_mpet_element_limit || {
    echo "not ok 1 - mpet rejects 257 elements"
    exit 0
}

echo "ok 1 - mpet rejects 257 elements"
exit 0
