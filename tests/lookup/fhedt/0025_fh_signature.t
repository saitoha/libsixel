#!/bin/sh
# Verify fhedt signature.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/fhedt/0015_fh_signature || {
    echo "not ok 1 - fhedt signature"
    exit 0
}

echo "ok 1 - fhedt signature"
exit 0
