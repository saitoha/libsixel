#!/bin/sh
# Verify mahalanobis linear fraction.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/mahalanobis/0007_mah_linear_fraction || {
    printf '\n%s\n' "not ok 1 - mahalanobis linear fraction"
    exit 0
}

printf '\n%s\n' "ok 1 - mahalanobis linear fraction"
exit 0
