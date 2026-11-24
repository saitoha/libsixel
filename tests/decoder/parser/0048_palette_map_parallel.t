#!/bin/sh
# Verify palette map parallel.
# Policy: docs/decoder/pipeline/README.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0046_palette_map_parallel" || {
    printf '\n%s\n' "not ok 1 - 0046_palette_map_parallel"
    exit 0
}

printf '\n%s\n' "ok 1 - 0046_palette_map_parallel"
exit 0
