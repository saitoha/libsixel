#!/bin/sh
set -eux
echo '1..1'
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    dither/fs-gpu/0015_auto_diffusion_gpu_policy keep normal || status=$?
test "$status" -ne 77 || {
    printf '\n%s\n' 'ok 1 # SKIP requires playback threads'
    exit 0
}
test "$status" -eq 0 || {
    printf '\n%s\n' 'not ok 1 - GPU playback keeps transparent pixels'
    exit 0
}
printf '\n%s\n' 'ok 1 - GPU playback keeps transparent pixels'
exit 0
