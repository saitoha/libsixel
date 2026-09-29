#!/bin/sh
# Verify eytzinger din99d oracle.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/eytzinger/0020_eyt_din99d_oracle || {
    echo "not ok 1 - eytzinger din99d oracle"
    exit 0
}

echo "ok 1 - eytzinger din99d oracle"
exit 0
