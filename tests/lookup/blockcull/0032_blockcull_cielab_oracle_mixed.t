#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/blockcull.md
# Verify blockcull cielab oracle mixed.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/blockcull/0029_blockcull_cielab_oracle_mixed || {
    printf '\n%s\n' "not ok 1 - blockcull cielab oracle mixed"
    exit 0
}
printf '\n%s\n' "ok 1 - blockcull cielab oracle mixed"
exit 0
