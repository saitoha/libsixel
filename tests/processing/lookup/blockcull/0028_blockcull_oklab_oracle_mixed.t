#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/blockcull.md
# Verify blockcull oklab oracle mixed.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/blockcull/0028_blockcull_oklab_oracle_mixed || {
    echo "not ok 1 - blockcull oklab oracle mixed"
    exit 0
}
echo "ok 1 - blockcull oklab oracle mixed"
exit 0
