#!/bin/sh
# Verify independent DIN99d reference values.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "colorspace/0001_din99d_forward" || {
    echo "not ok 1 - 0001_din99d_forward"
    exit 0
}

echo "ok 1 - 0001_din99d_forward"
exit 0
