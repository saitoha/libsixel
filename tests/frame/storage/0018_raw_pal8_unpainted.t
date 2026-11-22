#!/bin/sh
# Verify that raw PAL8 preserves the unpainted pixel during conversion.
# Policy: docs/concepts/pixelformat-precision/README.md
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0014_clone_float_canonical raw-palette || {
    echo 'not ok 1 - raw PAL8 preserves the unpainted pixel during conversion'
    exit 0
}
echo 'ok 1 - raw PAL8 preserves the unpainted pixel during conversion'
exit 0
