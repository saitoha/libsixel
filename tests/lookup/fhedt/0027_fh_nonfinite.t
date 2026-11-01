#!/bin/sh
# Verify fhedt nonfinite.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/fhedt/0017_fh_nonfinite || {
    echo "not ok 1 - fhedt nonfinite"
    exit 0
}

echo "ok 1 - fhedt nonfinite"
exit 0
