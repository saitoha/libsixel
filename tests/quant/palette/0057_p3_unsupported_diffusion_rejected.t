#!/bin/sh
# Verify p3 unsupported diffusion rejected.
# Policy: docs/functionality/output-colorspace.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/0054_p3_encoder_contract diffusion || {
    echo "not ok 1 - p3 unsupported diffusion rejected"
    exit 0
}

echo "ok 1 - p3 unsupported diffusion rejected"
exit 0
