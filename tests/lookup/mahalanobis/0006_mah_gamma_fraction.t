#!/bin/sh
# Verify mahalanobis gamma fraction.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/mahalanobis/0006_mah_gamma_fraction || {
    echo "not ok 1 - mahalanobis gamma fraction"
    exit 0
}

echo "ok 1 - mahalanobis gamma fraction"
exit 0
