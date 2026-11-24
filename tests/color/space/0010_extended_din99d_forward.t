#!/bin/sh
# Verify extended din99d forward against independent fixtures.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "colorspace/0010_extended_din99d_forward" || {
    printf '\n%s\n' "not ok 1 - extended din99d forward"
    exit 0
}

printf '\n%s\n' "ok 1 - extended din99d forward"
exit 0
