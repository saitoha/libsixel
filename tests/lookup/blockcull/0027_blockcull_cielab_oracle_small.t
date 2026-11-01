#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/blockcull.md
# Verify blockcull cielab oracle small.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/blockcull/0024_blockcull_cielab_oracle_small || {
    echo "not ok 1 - blockcull cielab oracle small"
    exit 0
}
echo "ok 1 - blockcull cielab oracle small"
exit 0
