#!/bin/sh
# srgb oklab fs residual.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/rgb101_diffusion/0001_spatial_contract \
    residual srgb oklab fs || {
    echo "not ok 1 - srgb oklab fs residual"
    exit 0
}
echo "ok 1 - srgb oklab fs residual"
exit 0
