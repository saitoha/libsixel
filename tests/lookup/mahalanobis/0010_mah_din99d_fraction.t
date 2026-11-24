#!/bin/sh
# Verify mahalanobis din99d fraction.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/mahalanobis/0010_mah_din99d_fraction || {
    printf '\n%s\n' "not ok 1 - mahalanobis din99d fraction"
    exit 0
}

printf '\n%s\n' "ok 1 - mahalanobis din99d fraction"
exit 0
