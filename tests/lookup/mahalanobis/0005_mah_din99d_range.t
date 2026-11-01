#!/bin/sh
# Verify mahalanobis din99d range.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/mahalanobis/0005_mah_din99d_range || {
    echo "not ok 1 - mahalanobis din99d range"
    exit 0
}

echo "ok 1 - mahalanobis din99d range"
exit 0
