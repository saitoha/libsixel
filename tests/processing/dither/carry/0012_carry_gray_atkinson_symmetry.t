#!/bin/sh
# Verify complementary gray bias symmetry for intentional 6/8 attenuation.
# Policy: docs/encoder/palette-application/dithering/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "dither/carry/0012_carry_gray_atkinson_symmetry" || {
    echo "not ok 1 - gray_atkinson_symmetry"
    exit 0
}
echo "ok 1 - gray_atkinson_symmetry"
exit 0
