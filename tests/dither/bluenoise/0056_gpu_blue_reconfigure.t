#!/bin/sh
# Policy: docs/encoder/palette-application/dithering/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" positional_gpu_blue_reconfigure || {
    printf '\n%s\n' "not ok 1 - GPU blue diffusion round trip"
    exit 0
}
printf '\n%s\n' "ok 1 - GPU blue diffusion round trip"
exit 0
