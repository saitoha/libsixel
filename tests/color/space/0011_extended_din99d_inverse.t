#!/bin/sh
# Verify extended din99d inverse against independent fixtures.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "colorspace/0011_extended_din99d_inverse" || {
    printf '\n%s\n' "not ok 1 - extended din99d inverse"
    exit 0
}

printf '\n%s\n' "ok 1 - extended din99d inverse"
exit 0
