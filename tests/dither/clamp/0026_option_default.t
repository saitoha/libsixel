#!/bin/sh
# Verify option default using fixed numeric expectations.
# Policy: docs/encoder/palette-application/clamp-policy.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "dither/clamp/0026_option_default" || {
    printf '\n%s\n' "not ok 1 - option_default"
    exit 0
}
printf '\n%s\n' "ok 1 - option_default"
exit 0
