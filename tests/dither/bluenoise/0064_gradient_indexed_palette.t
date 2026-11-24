#!/bin/sh
# The gradient map uses palette luminance for indexed pixels.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" dither/bluenoise/0064_gradient_indexed_palette || {
    printf '\n%s\n' "not ok 1 - the gradient map uses palette luminance"
    exit 0
}
printf '\n%s\n' "ok 1 - the gradient map uses palette luminance"
exit 0
