#!/bin/sh
# Packed palette expansion preserves keyed and masked pixels.
# Policy: docs/concepts/pixelformat-precision/README.md
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0014_clone_float_canonical packed-byte-mask || {
    echo 'not ok 1 - Packed palette expansion preserves keyed and masked pixels'
    exit 0
}
echo 'ok 1 - Packed palette expansion preserves keyed and masked pixels'
exit 0
