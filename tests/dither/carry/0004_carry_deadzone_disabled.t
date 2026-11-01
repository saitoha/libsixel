#!/bin/sh
# Verify deadzone disabled using explicit numeric expectations.
# Policy: docs/encoder/palette-application/dithering/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "dither/carry/0004_carry_deadzone_disabled" || {
    echo "not ok 1 - deadzone_disabled"
    exit 0
}
echo "ok 1 - deadzone_disabled"
exit 0
