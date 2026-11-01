#!/bin/sh
# Verify mahalanobis gamma range.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/mahalanobis/0001_mah_gamma_range || {
    echo "not ok 1 - mahalanobis gamma range"
    exit 0
}

echo "ok 1 - mahalanobis gamma range"
exit 0
