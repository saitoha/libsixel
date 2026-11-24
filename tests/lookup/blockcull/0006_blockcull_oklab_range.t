#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/blockcull.md
# Verify blockcull oklab range.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/blockcull/0003_blockcull_oklab_range || {
    printf '\n%s\n' "not ok 1 - blockcull oklab range"
    exit 0
}
printf '\n%s\n' "ok 1 - blockcull oklab range"
exit 0
