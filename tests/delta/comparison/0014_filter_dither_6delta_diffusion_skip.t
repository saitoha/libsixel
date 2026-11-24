#!/bin/sh
# Run the 6delta diffusion skip regression via the unified runner.
# Policy: docs/encoder/output/delta-encoding.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "filter/0026_filter_dither_6delta_diffusion_skip" || {
    printf '\n%s\n' "not ok 1 - 0026_filter_dither_6delta_diffusion_skip"
    exit 0
}

printf '\n%s\n' "ok 1 - 0026_filter_dither_6delta_diffusion_skip"
exit 0
