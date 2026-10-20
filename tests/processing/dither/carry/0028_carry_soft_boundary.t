#!/bin/sh
# The limiting component reaches its rounded integer boundary.
# Policy: docs/encoder/palette-application/dithering/README.md
# Policy: docs/encoder/palette-application/clamp-policy.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "dither/carry/0028_carry_soft_boundary" || {
    echo "not ok 1 - carry_soft_boundary"
    exit 0
}
echo "ok 1 - carry_soft_boundary"
exit 0
