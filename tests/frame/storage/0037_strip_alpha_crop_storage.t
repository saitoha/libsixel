#!/bin/sh
# Stripping alpha in place retains the original pixel allocation.
# Policy: docs/concepts/pixelformat-precision/README.md
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0014_clone_float_canonical strip-storage || {
    echo 'not ok 1 - Stripping alpha in place retains the original pixel allocation'
    exit 0
}
echo 'ok 1 - Stripping alpha in place retains the original pixel allocation'
exit 0
