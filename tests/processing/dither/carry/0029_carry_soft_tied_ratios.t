#!/bin/sh
# Equal positive and negative ratios preserve nearest-even half ties.
# Policy: docs/encoder/palette-application/dithering/README.md
# Policy: docs/encoder/palette-application/clamp-policy.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "dither/carry/0029_carry_soft_tied_ratios" || {
    echo "not ok 1 - carry_soft_tied_ratios"
    exit 0
}
echo "ok 1 - carry_soft_tied_ratios"
exit 0
