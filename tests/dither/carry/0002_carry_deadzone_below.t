#!/bin/sh
# Verify deadzone below using explicit numeric expectations.
# Policy: docs/encoder/palette-application/dithering/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "dither/carry/0002_carry_deadzone_below" || {
    echo "not ok 1 - deadzone_below"
    exit 0
}
echo "ok 1 - deadzone_below"
exit 0
