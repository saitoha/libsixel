#!/bin/sh
# Verify eytzinger gamma oracle_small.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/eytzinger/0026_eyt_gamma_oracle_small || {
    echo "not ok 1 - eytzinger gamma oracle_small"
    exit 0
}

echo "ok 1 - eytzinger gamma oracle_small"
exit 0
