#!/bin/sh
# Verify palette map definitions.
# Policy: docs/decoder/pipeline/README.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0043_palette_map_definitions" || {
    printf '\n%s\n' "not ok 1 - 0043_palette_map_definitions"
    exit 0
}

printf '\n%s\n' "ok 1 - 0043_palette_map_definitions"
exit 0
