#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/blockcull.md
# Verify blockcull gamma oracle.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/blockcull/0011_blockcull_gamma_oracle || {
    echo "not ok 1 - blockcull gamma oracle"
    exit 0
}
echo "ok 1 - blockcull gamma oracle"
exit 0
