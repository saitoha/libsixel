#!/bin/sh
# Verify mahalanobis gamma oracle_large.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/mahalanobis/0021_mah_gamma_oracle_large || {
    printf '\n%s\n' "not ok 1 - mahalanobis gamma oracle_large"
    exit 0
}

printf '\n%s\n' "ok 1 - mahalanobis gamma oracle_large"
exit 0
