#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/rbc.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" gpu/rbc_fractional_cache || {
    printf '\n%s\n' "not ok 1 - rbc fractional cache"
    exit 0
}
printf '\n%s\n' "ok 1 - rbc fractional cache"
exit 0
