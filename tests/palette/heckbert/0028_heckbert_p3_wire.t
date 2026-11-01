#!/bin/sh
# Verify native Heckbert p3 wire.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=lookup_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/heckbert_float/0028_heckbert_p3_wire >/dev/null || {
    echo "not ok 1 - native Heckbert p3 wire"
    exit 0
}

echo "ok 1 - native Heckbert p3 wire"
exit 0
