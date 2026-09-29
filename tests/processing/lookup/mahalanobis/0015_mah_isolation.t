#!/bin/sh
# Verify mahalanobis isolation.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/mahalanobis/0015_mah_isolation || {
    echo "not ok 1 - mahalanobis isolation"
    exit 0
}

echo "ok 1 - mahalanobis isolation"
exit 0
