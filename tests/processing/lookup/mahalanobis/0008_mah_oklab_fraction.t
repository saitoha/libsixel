#!/bin/sh
# Verify mahalanobis oklab fraction.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/mahalanobis/0008_mah_oklab_fraction || {
    echo "not ok 1 - mahalanobis oklab fraction"
    exit 0
}

echo "ok 1 - mahalanobis oklab fraction"
exit 0
