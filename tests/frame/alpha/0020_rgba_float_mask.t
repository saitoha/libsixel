#!/bin/sh
# Float promotion preserves zero alpha in the decoded paint mask.
# Policy: docs/concepts/pixelformat-precision/README.md
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0014_clone_float_canonical rgba-float-mask || {
    echo 'not ok 1 - Float promotion preserves zero alpha in the decoded paint mask'
    exit 0
}
echo 'ok 1 - Float promotion preserves zero alpha in the decoded paint mask'
exit 0
