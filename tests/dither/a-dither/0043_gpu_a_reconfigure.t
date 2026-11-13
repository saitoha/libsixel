#!/bin/sh
# Policy: docs/encoder/palette-application/dithering/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" positional_gpu_a_reconfigure || {
    echo "not ok 1 - GPU a diffusion round trip"
    exit 0
}
echo "ok 1 - GPU a diffusion round trip"
exit 0
