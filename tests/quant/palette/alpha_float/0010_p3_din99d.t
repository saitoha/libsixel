#!/bin/sh
# Verify native alpha: p3 din99d.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_THREADING_GLOBAL_BUDGET=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" quant/palette/alpha_float/0010_p3_din99d || {
    echo "not ok 1 - native alpha p3 din99d"
    exit 0
}

echo "ok 1 - native alpha p3 din99d"
exit 0
