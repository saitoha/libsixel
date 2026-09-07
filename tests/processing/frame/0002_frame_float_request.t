#!/bin/sh
# Run the focused frame float-request failure test.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "frame/0002_frame_float_request" || {
    echo "not ok 1 - frame float-request failure contract"
    exit 0
}

echo "ok 1 - frame float-request failure contract"
exit 0
