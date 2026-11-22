#!/bin/sh
# A shared blue primary preserves red and green rounding ties.
# Policy: docs/concepts/pixelformat-precision/README.md
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0014_clone_float_canonical shared-blue || {
    echo 'not ok 1 - A shared blue primary preserves red and green rounding ties'
    exit 0
}
echo 'ok 1 - A shared blue primary preserves red and green rounding ties'
exit 0
