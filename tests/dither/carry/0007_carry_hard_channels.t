#!/bin/sh
# Verify hard channels using explicit numeric expectations.
# Policy: docs/encoder/palette-application/dithering/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "dither/carry/0007_carry_hard_channels" || {
    echo "not ok 1 - hard_channels"
    exit 0
}
echo "ok 1 - hard_channels"
exit 0
