#!/bin/sh
# Check that the sRGB frame retains its red primary through clone and crop.
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0014_clone_float_canonical srgb || {
    echo 'not ok 1 - sRGB frame retains its red primary through clone and crop'
    exit 0
}
echo 'ok 1 - sRGB frame retains its red primary through clone and crop'
exit 0
