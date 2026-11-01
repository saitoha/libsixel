#!/bin/sh
# Verify that hard and soft binning execute through the filter vtable.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "filter/0031_filter_binning" || {
    echo "not ok 1 - binning filter contract"
    exit 0
}

echo "ok 1 - binning filter contract"
exit 0
