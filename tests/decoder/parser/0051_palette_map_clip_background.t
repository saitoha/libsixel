#!/bin/sh
# Verify palette map clip background.
# Policy: docs/decoder/pipeline/README.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0049_palette_map_clip_background" || {
    printf '\n%s\n' "not ok 1 - 0049_palette_map_clip_background"
    exit 0
}

printf '\n%s\n' "ok 1 - 0049_palette_map_clip_background"
exit 0
