#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/blockcull.md
# Verify blockcull linear oracle.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/blockcull/0012_blockcull_linear_oracle || {
    echo "not ok 1 - blockcull linear oracle"
    exit 0
}
echo "ok 1 - blockcull linear oracle"
exit 0
