#!/bin/sh
# Verify mapped palette during parallel fallback.
# Policy: docs/decoder/pipeline/README.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0052_palette_map_parallel_fallback" || {
    printf '\n%s\n' "not ok 1 - 0052_palette_map_parallel_fallback"
    exit 0
}

printf '\n%s\n' "ok 1 - 0052_palette_map_parallel_fallback"
exit 0
