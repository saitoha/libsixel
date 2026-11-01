#!/bin/sh
# Verify mahalanobis oklab oracle.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/mahalanobis/0018_mah_oklab_oracle || {
    echo "not ok 1 - mahalanobis oklab oracle"
    exit 0
}

echo "ok 1 - mahalanobis oklab oracle"
exit 0
