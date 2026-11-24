#!/bin/sh
# Run the focused frame transparent-mask clip test.
# Policy: docs/concepts/pixelformat.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "frame/0003_frame_transparent_mask_clip" || {
    printf '\n%s\n' "not ok 1 - frame transparent-mask clip contract"
    exit 0
}

printf '\n%s\n' "ok 1 - frame transparent-mask clip contract"
exit 0
