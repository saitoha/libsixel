#!/bin/sh
# Run the 6delta retained-plane regression via the unified runner.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "filter/0012_filter_encode_6delta_plane" || {
    echo "not ok 1 - 0012_filter_encode_6delta_plane"
    exit 0
}

echo "ok 1 - 0012_filter_encode_6delta_plane"
exit 0
