#!/bin/sh
# Verify palette map identity.
# Policy: docs/decoder/pipeline/README.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0050_palette_map_identity" || {
    printf '\n%s\n' "not ok 1 - 0050_palette_map_identity"
    exit 0
}

printf '\n%s\n' "ok 1 - 0050_palette_map_identity"
exit 0
