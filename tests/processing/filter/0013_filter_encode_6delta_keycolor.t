#!/bin/sh
# Run the 6delta retained-plane regression via the unified runner.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "filter/0013_filter_encode_6delta_keycolor" || {
    echo "not ok 1 - 0013_filter_encode_6delta_keycolor"
    exit 0
}

echo "ok 1 - 0013_filter_encode_6delta_keycolor"
exit 0
