#!/bin/sh
# Verify eytzinger oklab oracle_small.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/eytzinger/0028_eyt_oklab_oracle_small || {
    echo "not ok 1 - eytzinger oklab oracle_small"
    exit 0
}

echo "ok 1 - eytzinger oklab oracle_small"
exit 0
