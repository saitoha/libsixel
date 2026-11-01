#!/bin/sh
# Verify native Heckbert din99d wire.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=lookup_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/heckbert_float/0027_heckbert_din99d_wire >/dev/null || {
    echo "not ok 1 - native Heckbert din99d wire"
    exit 0
}

echo "ok 1 - native Heckbert din99d wire"
exit 0
