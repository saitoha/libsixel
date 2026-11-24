#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/blockcull.md
# Verify blockcull gamma oracle small.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/blockcull/0021_blockcull_gamma_oracle_small || {
    printf '\n%s\n' "not ok 1 - blockcull gamma oracle small"
    exit 0
}
printf '\n%s\n' "ok 1 - blockcull gamma oracle small"
exit 0
