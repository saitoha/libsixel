#!/bin/sh
# Run the focused frame transparent-mask resize test.
# Policy: docs/concepts/pixelformat.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "frame/0004_frame_transparent_mask_resize" || {
    echo "not ok 1 - frame transparent-mask resize contract"
    exit 0
}

echo "ok 1 - frame transparent-mask resize contract"
exit 0
