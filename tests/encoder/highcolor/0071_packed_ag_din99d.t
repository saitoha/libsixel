#!/bin/sh
# Alpha-gray honors the background through DIN99d and P3.
# Policy: docs/encoder/output/high-color/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" codec/highcolor/0071_packed_ag_din99d || {
    printf '\n%s\n' "not ok 1 - high-color packed ag din99d"
    exit 0
}
printf '\n%s\n' "ok 1 - high-color packed ag din99d"
exit 0
