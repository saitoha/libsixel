#!/bin/sh
# Builtin SIXEL frame storage includes its retained byte palette.
# Policy: docs/concepts/pixelformat-precision/README.md
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0014_clone_float_canonical raw-storage || {
    printf '\n%s\n' 'not ok 1 - Builtin SIXEL frame storage includes its retained byte palette'
    exit 0
}
printf '\n%s\n' 'ok 1 - Builtin SIXEL frame storage includes its retained byte palette'
exit 0
