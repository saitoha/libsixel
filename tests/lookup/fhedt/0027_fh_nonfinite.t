#!/bin/sh
# Verify fhedt nonfinite.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/fhedt/0017_fh_nonfinite || {
    printf '\n%s\n' "not ok 1 - fhedt nonfinite"
    exit 0
}

printf '\n%s\n' "ok 1 - fhedt nonfinite"
exit 0
