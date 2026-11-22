#!/bin/sh
# Gamma working space keeps both Display P3 colors distinct after decoding.
# Policy: docs/encoder/palette-application/working-colorspace/README.md
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" planner/0003_default_colorspace_auto gamma-p3 || {
    echo 'not ok 1 - Gamma working space keeps both Display P3 colors distinct after decoding'
    exit 0
}
echo 'ok 1 - Gamma working space keeps both Display P3 colors distinct after decoding'
exit 0
