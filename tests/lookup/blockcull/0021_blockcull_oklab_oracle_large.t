#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/blockcull.md
# Verify blockcull oklab oracle large.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/blockcull/0018_blockcull_oklab_oracle_large || {
    printf '\n%s\n' "not ok 1 - blockcull oklab oracle large"
    exit 0
}
printf '\n%s\n' "ok 1 - blockcull oklab oracle large"
exit 0
