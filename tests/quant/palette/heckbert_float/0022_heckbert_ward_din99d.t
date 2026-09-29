#!/bin/sh
# Verify native Heckbert ward din99d.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/heckbert_float/0022_heckbert_ward_din99d >/dev/null || {
    echo "not ok 1 - native Heckbert ward din99d"
    exit 0
}

echo "ok 1 - native Heckbert ward din99d"
exit 0
