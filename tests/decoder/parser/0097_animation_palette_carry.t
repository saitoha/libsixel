#!/bin/sh
# Policy: docs/quality/video-evaluation.md
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "decoder/0087_animation_palette_carry" || {
    echo "not ok 1 - animation_palette_carry"
    exit 0
}
echo "ok 1 - animation_palette_carry"
exit 0
