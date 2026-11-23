#!/bin/sh
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    frame/0014_clone_float_canonical packed-rgb-matrix || {
    echo 'not ok 1 - Packed RGB converts all supported layouts'
    exit 0
}
echo 'ok 1 - Packed RGB converts all supported layouts'
exit 0
