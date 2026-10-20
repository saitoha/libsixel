#!/bin/sh
# Hard candidates match the frozen double reference.
# Policy: docs/encoder/palette-application/dithering/README.md
# Policy: docs/encoder/palette-application/clamp-policy.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "dither/carry/0026_carry_hard_double" || {
    echo "not ok 1 - carry_hard_double"
    exit 0
}
echo "ok 1 - carry_hard_double"
exit 0
