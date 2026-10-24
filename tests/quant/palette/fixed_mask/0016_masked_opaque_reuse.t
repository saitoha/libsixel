#!/bin/sh
# Verify fixed palette mask: masked opaque reuse.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_THREADING_GLOBAL_BUDGET=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" quant/palette/fixed_mask/0016_masked_opaque_reuse || {
    echo "not ok 1 - fixed palette mask masked opaque reuse"
    exit 0
}

echo "ok 1 - fixed palette mask masked opaque reuse"
exit 0
