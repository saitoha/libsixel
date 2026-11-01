#!/bin/sh
# Carry allocation failure returns an error before output or source changes.
# Policy: docs/encoder/palette-application/dithering/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "dither/carry/0020_carry_allocation_failure" || {
    echo "not ok 1 - allocation_failure"
    exit 0
}
echo "ok 1 - allocation_failure"
exit 0
