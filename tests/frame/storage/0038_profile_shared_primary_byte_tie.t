#!/bin/sh
# A shared blue primary preserves red and green rounding ties.
# Policy: docs/concepts/pixelformat-precision/README.md
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0014_clone_float_canonical shared-blue || {
    printf '\n%s\n' 'not ok 1 - A shared blue primary preserves red and green rounding ties'
    exit 0
}
printf '\n%s\n' 'ok 1 - A shared blue primary preserves red and green rounding ties'
exit 0
