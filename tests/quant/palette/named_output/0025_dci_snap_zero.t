#!/bin/sh
# Verify named destination dci zero-rate snap identity.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_ENCODER_THREADING_POLICY=greedy SIXEL_ENCODER_THREADING_GLOBAL_BUDGET=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" quant/palette/named_output/0025_dci_snap_zero || {
    echo "not ok 1 - named destination dci zero-rate snap identity"
    exit 0
}

echo "ok 1 - named destination dci zero-rate snap identity"
exit 0
