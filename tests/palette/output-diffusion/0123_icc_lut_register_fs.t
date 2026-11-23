#!/bin/sh
# Policy: docs/encoder/output/output-colorspace.md
# Diffuse error from the emitted RGB codes evaluated by the forward LUT.
set -eux
echo '1..1'
set -v

SIXEL_ENCODER_THREADING_POLICY=greedy \
SIXEL_ENCODER_THREADING_GLOBAL_BUDGET=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/icc_output/0001_icc_output register-feedback 1 \
    "${TOP_SRCDIR}/tests/data/color-numeric" || {
    echo 'not ok 1 - Diffuse error from the emitted RGB codes evaluated by the forward LUT'
    exit 0
}
echo 'ok 1 - Diffuse error from the emitted RGB codes evaluated by the forward LUT'
exit 0
