#!/bin/sh
# All spatial option scopes accept decimals, reject invalid values and reset defaults.
# Policy: docs/encoder/palette-application/dithering/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "dither/carry/0019_carry_option_scope" || {
    printf '\n%s\n' "not ok 1 - option_scope"
    exit 0
}
printf '\n%s\n' "ok 1 - option_scope"
exit 0
