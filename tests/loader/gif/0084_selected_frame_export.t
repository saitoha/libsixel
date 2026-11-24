#!/bin/sh
# Verify skipped GIF frames compose without exporting their palettes.
set -eux
echo '1..1'
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" gif/selected_frame_export || {
    printf '\n%s\n' 'not ok 1 - only the selected GIF frame is exported'
    exit 0
}
printf '\n%s\n' 'ok 1 - only the selected GIF frame is exported'
exit 0
