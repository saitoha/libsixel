#!/bin/sh
# Verify mahalanobis oklab oracle_large.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/mahalanobis/0023_mah_oklab_oracle_large || {
    echo "not ok 1 - mahalanobis oklab oracle_large"
    exit 0
}

echo "ok 1 - mahalanobis oklab oracle_large"
exit 0
