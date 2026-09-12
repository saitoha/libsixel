#!/bin/sh
# Run the Sierra Two-row 6delta regression via the unified runner.
# Policy: docs/functionality/delta-encoding.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "filter/0020_filter_dither_6delta_sierra2" || {
    echo "not ok 1 - 0020_filter_dither_6delta_sierra2"
    exit 0
}

echo "ok 1 - 0020_filter_dither_6delta_sierra2"
exit 0
