#!/bin/sh
# Policy: docs/encoder/palette-construction/pipeline/README.md
# Verify automatic sampling keeps the palette of re-encoded PAL8 frames.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0081_sampling_auto_pal8_retention" || {
    echo "not ok 1 - automatic sampling keeps PAL8 palettes"
    exit 0
}
echo "ok 1 - automatic sampling keeps PAL8 palettes"
exit 0
