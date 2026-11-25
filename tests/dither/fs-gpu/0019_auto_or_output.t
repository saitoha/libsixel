#!/bin/sh
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    dither/fs-gpu/0015_auto_diffusion_gpu_policy or || {
    printf '\n%s\n' 'not ok 1 - GPU auto preserves or output colors'
    exit 0
}
printf '\n%s\n' 'ok 1 - GPU auto preserves or output colors'
exit 0
