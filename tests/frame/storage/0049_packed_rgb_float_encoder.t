#!/bin/sh
# Packed RGB matches RGB888 in float working spaces.
# Policy: docs/concepts/pixelformat-precision/README.md
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0014_clone_float_canonical packed-float-output || {
    echo 'not ok 1 - Packed RGB matches RGB888 in float working spaces'
    exit 0
}
echo 'ok 1 - Packed RGB matches RGB888 in float working spaces'
exit 0
