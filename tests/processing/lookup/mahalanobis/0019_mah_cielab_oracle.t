#!/bin/sh
# Verify mahalanobis cielab oracle.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/mahalanobis/0019_mah_cielab_oracle || {
    echo "not ok 1 - mahalanobis cielab oracle"
    exit 0
}

echo "ok 1 - mahalanobis cielab oracle"
exit 0
