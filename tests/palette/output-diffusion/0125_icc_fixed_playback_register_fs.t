#!/bin/sh
# Policy: docs/encoder/output/output-colorspace.md
# Reuse the fixed-palette ICC codes and working colors across playback frames.
set -eux
echo '1..1'
set -v

# The C probe captures SIXEL itself. Keep terminal cursor moves out of TAP.
SIXEL_ENCODER_THREADING_POLICY=greedy \
SIXEL_ENCODER_THREADING_GLOBAL_BUDGET=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/icc_output/0001_icc_output register-feedback 3 \
    "${TOP_SRCDIR}/tests/data/color-numeric" >/dev/null || {
    echo 'not ok 1 - Reuse the fixed-palette ICC codes and working colors across playback frames'
    exit 0
}
echo 'ok 1 - Reuse the fixed-palette ICC codes and working colors across playback frames'
exit 0
