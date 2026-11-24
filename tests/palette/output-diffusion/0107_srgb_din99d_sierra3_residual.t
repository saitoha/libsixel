#!/bin/sh
# srgb din99d sierra3 residual.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/rgb101_diffusion/0001_spatial_contract \
    residual srgb din99d sierra3 || {
    printf '\n%s\n' "not ok 1 - srgb din99d sierra3 residual"
    exit 0
}
printf '\n%s\n' "ok 1 - srgb din99d sierra3 residual"
exit 0
