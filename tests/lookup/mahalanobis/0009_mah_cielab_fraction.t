#!/bin/sh
# Verify mahalanobis cielab fraction.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/mahalanobis/0009_mah_cielab_fraction || {
    printf '\n%s\n' "not ok 1 - mahalanobis cielab fraction"
    exit 0
}

printf '\n%s\n' "ok 1 - mahalanobis cielab fraction"
exit 0
