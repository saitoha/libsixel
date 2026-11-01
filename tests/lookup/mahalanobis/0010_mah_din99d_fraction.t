#!/bin/sh
# Verify mahalanobis din99d fraction.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/mahalanobis/0010_mah_din99d_fraction || {
    echo "not ok 1 - mahalanobis din99d fraction"
    exit 0
}

echo "ok 1 - mahalanobis din99d fraction"
exit 0
