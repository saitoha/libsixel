#!/bin/sh
# SMPTE-C byte samples convert to gamma sRGB RGBFLOAT32 values and tags.
# Policy: docs/concepts/pixelformat-precision/README.md
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0014_clone_float_canonical smptec-byte rgbfloat32 || {
    printf '\n%s\n' 'not ok 1 - SMPTE-C byte samples convert to gamma sRGB RGBFLOAT32 values and tags'
    exit 0
}
printf '\n%s\n' 'ok 1 - SMPTE-C byte samples convert to gamma sRGB RGBFLOAT32 values and tags'
exit 0
