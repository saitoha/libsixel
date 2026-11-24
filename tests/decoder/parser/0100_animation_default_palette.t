#!/bin/sh
# Policy: docs/quality/video-evaluation.md
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "decoder/0090_animation_default_palette" || {
    printf '\n%s\n' "not ok 1 - animation_default_palette"
    exit 0
}
printf '\n%s\n' "ok 1 - animation_default_palette"
exit 0
