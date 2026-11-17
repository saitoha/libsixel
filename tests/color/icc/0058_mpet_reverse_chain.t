#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0058_mpet_reverse_chain || {
    echo "not ok 1 - B2D evaluates the saturation pipeline in element order"
    exit 0
}

echo "ok 1 - B2D evaluates the saturation pipeline in element order"
exit 0
