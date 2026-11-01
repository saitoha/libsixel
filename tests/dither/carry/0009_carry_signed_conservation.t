#!/bin/sh
# Verify signed conservation using explicit numeric expectations.
# Policy: docs/encoder/palette-application/dithering/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "dither/carry/0009_carry_signed_conservation" || {
    echo "not ok 1 - signed_conservation"
    exit 0
}
echo "ok 1 - signed_conservation"
exit 0
