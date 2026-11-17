#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0040_mpet_element_boundary || {
    echo "not ok 1 - mpet accepts 256 elements"
    exit 0
}

echo "ok 1 - mpet accepts 256 elements"
exit 0
