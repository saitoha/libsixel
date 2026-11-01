#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/ordered.md
# Verify ordered din99d fraction.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/ordered/0010_ord_din99d_fraction || {
    echo "not ok 1 - ordered din99d fraction"
    exit 0
}
echo "ok 1 - ordered din99d fraction"
exit 0
