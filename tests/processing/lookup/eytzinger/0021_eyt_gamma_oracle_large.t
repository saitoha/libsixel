#!/bin/sh
# Verify eytzinger gamma oracle_large.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/eytzinger/0021_eyt_gamma_oracle_large || {
    echo "not ok 1 - eytzinger gamma oracle_large"
    exit 0
}

echo "ok 1 - eytzinger gamma oracle_large"
exit 0
