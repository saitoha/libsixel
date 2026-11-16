#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0042_mpet_element_kinds || {
    echo "not ok 1 - mpet recognizes the standard element headers"
    exit 0
}

echo "ok 1 - mpet recognizes the standard element headers"
exit 0
