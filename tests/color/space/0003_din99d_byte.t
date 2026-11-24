#!/bin/sh
# Verify independent DIN99d reference values.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "colorspace/0003_din99d_byte" || {
    printf '\n%s\n' "not ok 1 - 0003_din99d_byte"
    exit 0
}

printf '\n%s\n' "ok 1 - 0003_din99d_byte"
exit 0
