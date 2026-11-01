#!/bin/sh
# Verify deadzone boundary using explicit numeric expectations.
# Policy: docs/encoder/palette-application/dithering/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "dither/carry/0003_carry_deadzone_boundary" || {
    echo "not ok 1 - deadzone_boundary"
    exit 0
}
echo "ok 1 - deadzone_boundary"
exit 0
