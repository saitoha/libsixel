#!/bin/sh
# Packed frame storage counts each padded row and palette.
# Policy: docs/concepts/pixelformat-precision/README.md
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0014_clone_float_canonical packed-storage || {
    echo 'not ok 1 - Packed frame storage counts each padded row and palette'
    exit 0
}
echo 'ok 1 - Packed frame storage counts each padded row and palette'
exit 0
