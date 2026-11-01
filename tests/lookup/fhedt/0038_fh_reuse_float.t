#!/bin/sh
# Verify fhedt reuse float isolation.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/fhedt/0028_fh_reuse_float || {
    echo "not ok 1 - fhedt reuse float isolation"
    exit 0
}

echo "ok 1 - fhedt reuse float isolation"
exit 0
