#!/bin/sh
# Verify fhedt outside tiny.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/fhedt/0019_fh_outside_tiny || {
    echo "not ok 1 - fhedt outside tiny"
    exit 0
}

echo "ok 1 - fhedt outside tiny"
exit 0
