#!/bin/sh
# Verify fhedt outside cache.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/fhedt/0009_fh_outside_cache || {
    echo "not ok 1 - fhedt outside cache"
    exit 0
}

echo "ok 1 - fhedt outside cache"
exit 0
