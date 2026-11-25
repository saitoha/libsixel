#!/bin/sh
# Keyed color frames emit the visible palette register.
# Policy: docs/concepts/pixelformat-precision/README.md
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0014_clone_float_canonical keyed-registers || {
    printf '\n%s\n' 'not ok 1 - Keyed color frames emit the visible palette register'
    exit 0
}
printf '\n%s\n' 'ok 1 - Keyed color frames emit the visible palette register'
exit 0
