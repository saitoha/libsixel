#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/blockcull.md
# Verify blockcull gamma oracle large.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/blockcull/0016_blockcull_gamma_oracle_large || {
    echo "not ok 1 - blockcull gamma oracle large"
    exit 0
}
echo "ok 1 - blockcull gamma oracle large"
exit 0
