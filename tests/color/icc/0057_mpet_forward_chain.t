#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0057_mpet_forward_chain || {
    printf '\n%s\n' "not ok 1 - D2B evaluates the relative pipeline in element order"
    exit 0
}

printf '\n%s\n' "ok 1 - D2B evaluates the relative pipeline in element order"
exit 0
