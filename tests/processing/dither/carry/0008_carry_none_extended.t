#!/bin/sh
# Verify none extended using explicit numeric expectations.
# Policy: docs/encoder/palette-application/dithering/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "dither/carry/0008_carry_none_extended" || {
    echo "not ok 1 - none_extended"
    exit 0
}
echo "ok 1 - none_extended"
exit 0
