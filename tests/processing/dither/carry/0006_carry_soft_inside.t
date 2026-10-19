#!/bin/sh
# Verify soft inside using explicit numeric expectations.
# Policy: docs/encoder/palette-application/dithering/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "dither/carry/0006_carry_soft_inside" || {
    echo "not ok 1 - soft_inside"
    exit 0
}
echo "ok 1 - soft_inside"
exit 0
