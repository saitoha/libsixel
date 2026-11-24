#!/bin/sh
# Run the focused bilinear frame transparent-mask resize test.
# Policy: docs/concepts/pixelformat.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "frame/0005_frame_transparent_mask_resize_bilinear" || {
    printf '\n%s\n' "not ok 1 - bilinear frame transparent-mask resize"
    exit 0
}

printf '\n%s\n' "ok 1 - bilinear frame transparent-mask resize"
exit 0
