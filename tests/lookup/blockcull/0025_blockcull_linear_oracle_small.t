#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/blockcull.md
# Verify blockcull linear oracle small.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/blockcull/0022_blockcull_linear_oracle_small || {
    echo "not ok 1 - blockcull linear oracle small"
    exit 0
}
echo "ok 1 - blockcull linear oracle small"
exit 0
