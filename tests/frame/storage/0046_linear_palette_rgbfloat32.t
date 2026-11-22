#!/bin/sh
# Linear palette entries expand to sRGB floats and visible SIXEL colors.
# Policy: docs/concepts/pixelformat-precision/README.md
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0014_clone_float_canonical palette-linear-float || {
    echo 'not ok 1 - Linear palette entries expand to sRGB floats and visible SIXEL colors'
    exit 0
}
echo 'ok 1 - Linear palette entries expand to sRGB floats and visible SIXEL colors'
exit 0
