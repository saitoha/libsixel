#!/bin/sh
# PAL8 promotion combines keyed and explicit coverage.
# Policy: docs/concepts/pixelformat-precision/README.md
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0014_clone_float_canonical indexed-mask || {
    printf '\n%s\n' 'not ok 1 - PAL8 promotion combines keyed and explicit coverage'
    exit 0
}
printf '\n%s\n' 'ok 1 - PAL8 promotion combines keyed and explicit coverage'
exit 0
