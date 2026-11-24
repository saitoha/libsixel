#!/bin/sh
# Verify srgb gamma lso2 wire against independent numeric expectations.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/rgb101_diffusion/0001_spatial_contract \
    wire srgb gamma lso2 || {
    printf '\n%s\n' "not ok 1 - srgb gamma lso2 wire"
    exit 0
}
printf '\n%s\n' "ok 1 - srgb gamma lso2 wire"
exit 0
