#!/bin/sh
# An in-place Display P3 crop retains the original pixel allocation.
# Policy: docs/concepts/pixelformat-precision/README.md
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0014_clone_float_canonical crop-storage || {
    echo 'not ok 1 - An in-place Display P3 crop retains the original pixel allocation'
    exit 0
}
echo 'ok 1 - An in-place Display P3 crop retains the original pixel allocation'
exit 0
