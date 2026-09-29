#!/bin/sh
# Verify fhedt instance isolation.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/fhedt/0023_fh_instance_isolation || {
    echo "not ok 1 - fhedt instance isolation"
    exit 0
}

echo "ok 1 - fhedt instance isolation"
exit 0
