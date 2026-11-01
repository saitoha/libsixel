#!/bin/sh
# Verify mahalanobis cielab oracle_small.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/mahalanobis/0029_mah_cielab_oracle_small || {
    echo "not ok 1 - mahalanobis cielab oracle_small"
    exit 0
}

echo "ok 1 - mahalanobis cielab oracle_small"
exit 0
