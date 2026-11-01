#!/bin/sh
# Compare specialized scan/scatter candidates with the frozen generic loop.
# Policy: docs/encoder/palette-application/clamp-policy.md
# Policy: docs/encoder/palette-application/dithering/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "dither/carry/0025_carry_rational_stencil" || {
    echo "not ok 1 - specialized stencil reference"
    exit 0
}
echo "ok 1 - specialized stencil reference"
exit 0
