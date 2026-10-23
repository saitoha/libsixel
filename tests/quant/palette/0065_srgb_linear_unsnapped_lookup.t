#!/bin/sh
# Linear sRGB lookup retains the unsnapped working palette.
# Policy: docs/encoder/output/output-colorspace.md
# Policy: docs/encoder/palette-construction/snap-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/0065_srgb_linear_unsnapped_lookup" || {
    echo "not ok 1 - srgb_linear_unsnapped_lookup"
    exit 0
}
echo "ok 1 - srgb_linear_unsnapped_lookup"
exit 0
