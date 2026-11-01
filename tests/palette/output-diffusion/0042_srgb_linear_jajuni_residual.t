#!/bin/sh
# Verify srgb linear jajuni residual against independent numeric expectations.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/rgb101_diffusion/0001_spatial_contract \
    residual srgb linear jajuni || {
    echo "not ok 1 - srgb linear jajuni residual"
    exit 0
}
echo "ok 1 - srgb linear jajuni residual"
exit 0
