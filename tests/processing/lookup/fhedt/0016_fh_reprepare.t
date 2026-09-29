#!/bin/sh
# Verify fhedt reprepare.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/fhedt/0016_fh_reprepare || {
    echo "not ok 1 - fhedt reprepare"
    exit 0
}

echo "ok 1 - fhedt reprepare"
exit 0
