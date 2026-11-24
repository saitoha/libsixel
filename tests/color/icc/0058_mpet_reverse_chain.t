#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0058_mpet_reverse_chain || {
    printf '\n%s\n' "not ok 1 - B2D evaluates the saturation pipeline in element order"
    exit 0
}

printf '\n%s\n' "ok 1 - B2D evaluates the saturation pipeline in element order"
exit 0
