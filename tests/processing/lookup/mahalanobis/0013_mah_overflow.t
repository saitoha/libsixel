#!/bin/sh
# Verify mahalanobis overflow.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/mahalanobis/0013_mah_overflow || {
    echo "not ok 1 - mahalanobis overflow"
    exit 0
}

echo "ok 1 - mahalanobis overflow"
exit 0
