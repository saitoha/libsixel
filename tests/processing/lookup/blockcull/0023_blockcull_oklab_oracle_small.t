#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/blockcull.md
# Verify blockcull oklab oracle small.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/blockcull/0023_blockcull_oklab_oracle_small || {
    echo "not ok 1 - blockcull oklab oracle small"
    exit 0
}
echo "ok 1 - blockcull oklab oracle small"
exit 0
