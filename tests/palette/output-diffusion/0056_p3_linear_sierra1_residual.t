#!/bin/sh
# Verify p3 linear sierra1 residual against independent numeric expectations.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/rgb101_diffusion/0001_spatial_contract \
    residual p3 linear sierra1 || {
    echo "not ok 1 - p3 linear sierra1 residual"
    exit 0
}
echo "ok 1 - p3 linear sierra1 residual"
exit 0
