#!/bin/sh
# Verify cached labels after reprepare.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

SIXEL_ENCODER_THREADING_POLICY=greedy SIXEL_ENCODER_THREADING_GLOBAL_BUDGET=1 ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/fhedt_byte/0008_cache_reprepare || {
    printf '\n%s\n' "not ok 1 - byte FHEDT cached labels after reprepare"
    exit 0
}

printf '\n%s\n' "ok 1 - byte FHEDT cached labels after reprepare"
exit 0
