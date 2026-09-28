#!/bin/sh
# Verify srgb gamma sierra1 residual against independent numeric expectations.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/rgb101_diffusion/0001_spatial_contract \
    residual srgb gamma sierra1 || {
    echo "not ok 1 - srgb gamma sierra1 residual"
    exit 0
}
echo "ok 1 - srgb gamma sierra1 residual"
exit 0
