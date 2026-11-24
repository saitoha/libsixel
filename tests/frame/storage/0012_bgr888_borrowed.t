#!/bin/sh
# Verify that the frame preserves borrowed BGR888 across allocation failure and retry.
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0001_frame_factory bgr888-borrowed || {
    printf '\n%s\n' 'not ok 1 - frame preserves borrowed BGR888 across allocation failure and retry'
    exit 0
}
printf '\n%s\n' 'ok 1 - frame preserves borrowed BGR888 across allocation failure and retry'
exit 0
