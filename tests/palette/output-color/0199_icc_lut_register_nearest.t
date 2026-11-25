#!/bin/sh
# Policy: docs/encoder/output/output-colorspace.md
# Use forward LUT evaluation of the emitted RGB codes for nearest search.
set -eux
echo '1..1'
set -v

SIXEL_ENCODER_THREADING_POLICY=greedy \
SIXEL_ENCODER_THREADING_GLOBAL_BUDGET=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/icc_output/0001_icc_output register-feedback 0 \
    "${TOP_SRCDIR}/tests/data/color-numeric" || {
    printf '\n%s\n' 'not ok 1 - Use forward LUT evaluation of the emitted RGB codes for nearest search'
    exit 0
}
printf '\n%s\n' 'ok 1 - Use forward LUT evaluation of the emitted RGB codes for nearest search'
exit 0
