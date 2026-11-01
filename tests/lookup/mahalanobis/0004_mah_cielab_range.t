#!/bin/sh
# Verify mahalanobis cielab range.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/mahalanobis/0004_mah_cielab_range || {
    echo "not ok 1 - mahalanobis cielab range"
    exit 0
}

echo "ok 1 - mahalanobis cielab range"
exit 0
