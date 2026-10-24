#!/bin/sh
# Verify named destination dci highcolor.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_ENCODER_THREADING_POLICY=greedy SIXEL_ENCODER_THREADING_GLOBAL_BUDGET=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" quant/palette/named_output/0023_dci_highcolor || {
    echo "not ok 1 - named destination dci highcolor"
    exit 0
}

echo "ok 1 - named destination dci highcolor"
exit 0
