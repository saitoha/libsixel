#!/bin/sh
# Verify eytzinger gamma mixed.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/eytzinger/0034_eyt_gamma_mixed || {
    echo "not ok 1 - eytzinger gamma mixed"
    exit 0
}

echo "ok 1 - eytzinger gamma mixed"
exit 0
