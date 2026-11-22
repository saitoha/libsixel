#!/bin/sh
# Permuted RGB primaries preserve exact channel independence.
# Policy: docs/concepts/pixelformat-precision/README.md
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0014_clone_float_canonical permuted-primaries || {
    echo 'not ok 1 - Permuted RGB primaries preserve exact channel independence'
    exit 0
}
echo 'ok 1 - Permuted RGB primaries preserve exact channel independence'
exit 0
