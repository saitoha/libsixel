#!/bin/sh
# Policy: docs/quality/video-evaluation.md
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "decoder/0090_animation_default_palette" || {
    echo "not ok 1 - animation_default_palette"
    exit 0
}
echo "ok 1 - animation_default_palette"
exit 0
