#!/bin/sh
# Verify option maximum using fixed numeric expectations.
# Policy: docs/encoder/palette-application/clamp-policy.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "dither/clamp/0031_option_maximum" || {
    printf '\n%s\n' "not ok 1 - option_maximum"
    exit 0
}
printf '\n%s\n' "ok 1 - option_maximum"
exit 0
