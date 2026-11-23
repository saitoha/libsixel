#!/bin/sh
set -eux
echo '1..1'
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    dither/fs-gpu/0015_auto_diffusion_gpu_policy mask-playback || status=$?
test "$status" -ne 77 || {
    echo 'ok 1 # SKIP requires playback threads'
    exit 0
}
test "$status" -eq 0 || {
    echo 'not ok 1 - GPU auto preserves paint coverage during playback'
    exit 0
}
echo 'ok 1 - GPU auto preserves paint coverage during playback'
exit 0
