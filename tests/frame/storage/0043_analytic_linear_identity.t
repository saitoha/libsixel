#!/bin/sh
# A linear sRGB definition keeps independent tiny channels exact.
# Policy: docs/concepts/pixelformat-precision/README.md
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0014_clone_float_canonical linear-identity || {
    printf '\n%s\n' 'not ok 1 - A linear sRGB definition keeps independent tiny channels exact'
    exit 0
}
printf '\n%s\n' 'ok 1 - A linear sRGB definition keeps independent tiny channels exact'
exit 0
