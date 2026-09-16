#!/bin/sh
# Verify extended din99d forward against independent fixtures.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "colorspace/0010_extended_din99d_forward" || {
    echo "not ok 1 - extended din99d forward"
    exit 0
}

echo "ok 1 - extended din99d forward"
exit 0
