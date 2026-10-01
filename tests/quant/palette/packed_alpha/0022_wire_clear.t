#!/bin/sh
# Policy: docs/encoder/output/output-colorspace.md
# Verify packed alpha: wire clear.
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_THREADS=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    quant/palette/packed_alpha/0022_wire_clear || {
    echo "not ok 1 - packed alpha wire clear"
    exit 0
}

echo "ok 1 - packed alpha wire clear"
exit 0
