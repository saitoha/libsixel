#!/bin/sh
# Verify srgb linear jajuni wire against independent numeric expectations.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/rgb101_diffusion/0001_spatial_contract \
    wire srgb linear jajuni || {
    echo "not ok 1 - srgb linear jajuni wire"
    exit 0
}
echo "ok 1 - srgb linear jajuni wire"
exit 0
