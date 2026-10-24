#!/bin/sh
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

SIXEL_DECODER_INPUT_COLORSPACE_ICC_PATH=/nonexistent.icc SIXEL_ENCODER_THREADING_POLICY=greedy SIXEL_ENCODER_THREADING_GLOBAL_BUDGET=1 ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/icc_output/0001_icc_output decoder 0 "icc:path=${TOP_SRCDIR}/tests/data/color-numeric/display-p3.icc" || {
    echo "not ok 1 - ICC input override"
    exit 0
}
echo "ok 1 - ICC input override"
exit 0
