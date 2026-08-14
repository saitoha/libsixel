#!/bin/sh
# Reject contradictory GPU 6delta request flags before engine selection.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "filter/0027_filter_gpu_6delta_disabled_reject" || {
    echo "not ok 1 - 0027_filter_gpu_6delta_disabled_reject"
    exit 0
}

echo "ok 1 - 0027_filter_gpu_6delta_disabled_reject"
exit 0
