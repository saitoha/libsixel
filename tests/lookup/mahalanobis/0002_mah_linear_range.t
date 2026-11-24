#!/bin/sh
# Verify mahalanobis linear range.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/mahalanobis/0002_mah_linear_range || {
    printf '\n%s\n' "not ok 1 - mahalanobis linear range"
    exit 0
}

printf '\n%s\n' "ok 1 - mahalanobis linear range"
exit 0
