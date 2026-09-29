#!/bin/sh
# Verify mahalanobis cielab metric.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/mahalanobis/0011_mah_cielab_metric || {
    echo "not ok 1 - mahalanobis cielab metric"
    exit 0
}

echo "ok 1 - mahalanobis cielab metric"
exit 0
