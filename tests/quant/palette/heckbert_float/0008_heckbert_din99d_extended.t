#!/bin/sh
# Verify native Heckbert din99d extended.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/heckbert_float/0008_heckbert_din99d_extended >/dev/null || {
    echo "not ok 1 - native Heckbert din99d extended"
    exit 0
}

echo "ok 1 - native Heckbert din99d extended"
exit 0
