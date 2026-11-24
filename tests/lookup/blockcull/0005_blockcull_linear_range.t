#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/blockcull.md
# Verify blockcull linear range.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/blockcull/0002_blockcull_linear_range || {
    printf '\n%s\n' "not ok 1 - blockcull linear range"
    exit 0
}
printf '\n%s\n' "ok 1 - blockcull linear range"
exit 0
