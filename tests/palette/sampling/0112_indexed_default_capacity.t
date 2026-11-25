#!/bin/sh
# Indexed frame encoding preserves register colors at default capacity.
# Policy: docs/concepts/pixelformat-precision/README.md
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" filter/0002_filter_sample indexed-capacity || {
    printf '\n%s\n' 'not ok 1 - Indexed frame encoding preserves register colors at default capacity'
    exit 0
}
printf '\n%s\n' 'ok 1 - Indexed frame encoding preserves register colors at default capacity'
exit 0
