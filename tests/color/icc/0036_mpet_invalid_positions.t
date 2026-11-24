#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0036_mpet_invalid_positions || {
    printf '\n%s\n' "not ok 1 - mpet rejects invalid element positions"
    exit 0
}

printf '\n%s\n' "ok 1 - mpet rejects invalid element positions"
exit 0
