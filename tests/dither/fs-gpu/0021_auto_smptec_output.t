#!/bin/sh
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    dither/fs-gpu/0015_auto_diffusion_gpu_policy smpte-c || {
    echo 'not ok 1 - GPU auto preserves smpte-c output colors'
    exit 0
}
echo 'ok 1 - GPU auto preserves smpte-c output colors'
exit 0
