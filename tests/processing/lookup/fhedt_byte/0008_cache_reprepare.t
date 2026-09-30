#!/bin/sh
# Verify cached labels after reprepare.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

SIXEL_THREADS=1 ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/fhedt_byte/0008_cache_reprepare || {
    echo "not ok 1 - byte FHEDT cached labels after reprepare"
    exit 0
}

echo "ok 1 - byte FHEDT cached labels after reprepare"
exit 0
