#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/blockcull.md
# Verify blockcull linear oracle large.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/blockcull/0017_blockcull_linear_oracle_large || {
    echo "not ok 1 - blockcull linear oracle large"
    exit 0
}
echo "ok 1 - blockcull linear oracle large"
exit 0
