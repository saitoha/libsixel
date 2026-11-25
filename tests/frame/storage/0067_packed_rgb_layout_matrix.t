#!/bin/sh
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    frame/0014_clone_float_canonical packed-rgb-matrix || {
    printf '\n%s\n' 'not ok 1 - Packed RGB converts all supported layouts'
    exit 0
}
printf '\n%s\n' 'ok 1 - Packed RGB converts all supported layouts'
exit 0
