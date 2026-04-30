#!/bin/sh
# Run the prebuilt frame factory unit test via the unified runner.
# This wrapper emits TAP based on exit status.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "frame/0001_frame_factory" || {
    echo "not ok 1 - 0001_frame_factory"
    exit 0
}

echo "ok 1 - 0001_frame_factory"
exit 0
