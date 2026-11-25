#!/bin/sh
# Profiled frames retry A2B1 at the float storage boundary.
# Policy: docs/concepts/pixelformat-precision/README.md
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0014_clone_float_canonical profiled-range || {
    printf '\n%s\n' 'not ok 1 - Profiled frames retry A2B1 at the float storage boundary'
    exit 0
}
printf '\n%s\n' 'ok 1 - Profiled frames retry A2B1 at the float storage boundary'
exit 0
