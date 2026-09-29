#!/bin/sh
# Verify mahalanobis linear oracle.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/mahalanobis/0017_mah_linear_oracle || {
    echo "not ok 1 - mahalanobis linear oracle"
    exit 0
}

echo "ok 1 - mahalanobis linear oracle"
exit 0
