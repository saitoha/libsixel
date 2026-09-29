#!/bin/sh
# Verify native Heckbert gamma wire.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=lookup_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/heckbert_float/0023_heckbert_gamma_wire >/dev/null || {
    echo "not ok 1 - native Heckbert gamma wire"
    exit 0
}

echo "ok 1 - native Heckbert gamma wire"
exit 0
