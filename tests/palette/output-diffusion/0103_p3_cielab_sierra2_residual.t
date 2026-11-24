#!/bin/sh
# p3 cielab sierra2 residual.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/rgb101_diffusion/0001_spatial_contract \
    residual p3 cielab sierra2 || {
    printf '\n%s\n' "not ok 1 - p3 cielab sierra2 residual"
    exit 0
}
printf '\n%s\n' "ok 1 - p3 cielab sierra2 residual"
exit 0
