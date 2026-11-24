#!/bin/sh
# Carry allocation failure returns an error before output or source changes.
# Policy: docs/encoder/palette-application/dithering/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "dither/carry/0020_carry_allocation_failure" || {
    printf '\n%s\n' "not ok 1 - allocation_failure"
    exit 0
}
printf '\n%s\n' "ok 1 - allocation_failure"
exit 0
