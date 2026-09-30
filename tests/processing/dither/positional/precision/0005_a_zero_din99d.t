#!/bin/sh
# Verify a zero din99d at the consuming boundary.
# Policy: docs/encoder/palette-application/dithering/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" dither/positional/precision/0005_a_zero_din99d || {
    echo "not ok 1 - a_zero_din99d"
    exit 0
}

echo "ok 1 - a_zero_din99d"
exit 0
