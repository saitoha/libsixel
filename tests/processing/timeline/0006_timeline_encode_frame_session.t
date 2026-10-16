#!/bin/sh
# Policy: docs/cli/diagnostics.md
# Pin the timeline session recorded by a direct encode_frame call.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "timeline/0006_timeline_encode_frame_session" || {
    echo "not ok 1 - encode_frame timeline session changed"
    exit 0
}

echo "ok 1 - encode_frame records a bracketed timeline session"
exit 0
