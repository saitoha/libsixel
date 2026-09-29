#!/bin/sh
# Verify native Heckbert cielab wire.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=lookup_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/heckbert_float/0026_heckbert_cielab_wire >/dev/null || {
    echo "not ok 1 - native Heckbert cielab wire"
    exit 0
}

echo "ok 1 - native Heckbert cielab wire"
exit 0
