#!/bin/sh
# Gamma sRGB lookup retains the unsnapped working palette.
# Policy: docs/encoder/output/output-colorspace.md
# Policy: docs/encoder/palette-construction/snap-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/0064_srgb_gamma_unsnapped_lookup" || {
    printf '\n%s\n' "not ok 1 - srgb_gamma_unsnapped_lookup"
    exit 0
}
printf '\n%s\n' "ok 1 - srgb_gamma_unsnapped_lookup"
exit 0
