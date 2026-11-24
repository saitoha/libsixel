#!/bin/sh
# Verify palette map dequant.
# Policy: docs/decoder/pipeline/README.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0047_palette_map_dequant" || {
    printf '\n%s\n' "not ok 1 - 0047_palette_map_dequant"
    exit 0
}

printf '\n%s\n' "ok 1 - 0047_palette_map_dequant"
exit 0
