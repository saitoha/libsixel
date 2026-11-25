#!/bin/sh
# Cropped indexed frames retain the original implicit palette capacity.
# Policy: docs/concepts/pixelformat-precision/README.md
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0014_clone_float_canonical implicit-palette-crop || {
    printf '\n%s\n' 'not ok 1 - Cropped indexed frames retain the original implicit palette capacity'
    exit 0
}
printf '\n%s\n' 'ok 1 - Cropped indexed frames retain the original implicit palette capacity'
exit 0
