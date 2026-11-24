#!/bin/sh
# Verify that the frame converts owned BGR888 without an allocation.
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0001_frame_factory bgr888-owned || {
    printf '\n%s\n' 'not ok 1 - frame converts owned BGR888 without an allocation'
    exit 0
}
printf '\n%s\n' 'ok 1 - frame converts owned BGR888 without an allocation'
exit 0
