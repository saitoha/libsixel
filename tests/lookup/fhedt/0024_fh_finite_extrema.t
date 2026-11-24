#!/bin/sh
# Verify fhedt finite extrema.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/fhedt/0014_fh_finite_extrema || {
    printf '\n%s\n' "not ok 1 - fhedt finite extrema"
    exit 0
}

printf '\n%s\n' "ok 1 - fhedt finite extrema"
exit 0
