#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0082_mpet_bmp_relative || {
    printf '\n%s\n' "not ok 1 - builtin BMP emits independent relative mpet samples"
    exit 0
}

printf '\n%s\n' "ok 1 - builtin BMP emits independent relative mpet samples"
exit 0
