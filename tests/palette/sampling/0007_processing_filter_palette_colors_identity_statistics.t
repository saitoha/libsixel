#!/bin/sh
# Verify palette colors identity statistics.
# Policy: docs/encoder/palette-construction/pipeline/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "filter/0049_palette_colors_identity_statistics" || {
    printf '\n%s\n' "not ok 1 - palette colors identity statistics"
    exit 0
}
printf '\n%s\n' "ok 1 - palette colors identity statistics"
exit 0
