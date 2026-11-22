#!/bin/sh
# A primary on the red-green edge contributes exactly zero blue.
# Policy: docs/concepts/pixelformat-precision/README.md
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0014_clone_float_canonical collinear || {
    echo 'not ok 1 - A primary on the red-green edge contributes exactly zero blue'
    exit 0
}
echo 'ok 1 - A primary on the red-green edge contributes exactly zero blue'
exit 0
