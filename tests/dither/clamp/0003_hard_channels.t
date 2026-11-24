#!/bin/sh
# Verify hard channels using fixed numeric expectations.
# Policy: docs/encoder/palette-application/clamp-policy.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "dither/clamp/0003_hard_channels" || {
    printf '\n%s\n' "not ok 1 - hard_channels"
    exit 0
}
printf '\n%s\n' "ok 1 - hard_channels"
exit 0
