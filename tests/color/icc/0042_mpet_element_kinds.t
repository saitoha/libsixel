#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0042_mpet_element_kinds || {
    printf '\n%s\n' "not ok 1 - mpet recognizes the standard element headers"
    exit 0
}

printf '\n%s\n' "ok 1 - mpet recognizes the standard element headers"
exit 0
