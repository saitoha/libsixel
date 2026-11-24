#!/bin/sh
# Verify palette map invalid.
# Policy: docs/decoder/pipeline/README.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0048_palette_map_invalid" || {
    printf '\n%s\n' "not ok 1 - 0048_palette_map_invalid"
    exit 0
}

printf '\n%s\n' "ok 1 - 0048_palette_map_invalid"
exit 0
