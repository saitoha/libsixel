#!/bin/sh
# Verify fhedt reuse byte isolation.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/fhedt/0027_fh_reuse_byte || {
    echo "not ok 1 - fhedt reuse byte isolation"
    exit 0
}

echo "ok 1 - fhedt reuse byte isolation"
exit 0
