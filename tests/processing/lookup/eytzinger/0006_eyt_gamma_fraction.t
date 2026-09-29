#!/bin/sh
# Verify eytzinger gamma fraction.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/eytzinger/0006_eyt_gamma_fraction || {
    echo "not ok 1 - eytzinger gamma fraction"
    exit 0
}

echo "ok 1 - eytzinger gamma fraction"
exit 0
