#!/bin/sh
# Check that the P3 frame retains its red primary through clone and crop.
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0014_clone_float_canonical p3 || {
    printf '\n%s\n' 'not ok 1 - P3 frame retains its red primary through clone and crop'
    exit 0
}
printf '\n%s\n' 'ok 1 - P3 frame retains its red primary through clone and crop'
exit 0
