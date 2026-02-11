#!/bin/sh
# Run the prebuilt filter unit test via the unified runner.
# This wrapper emits TAP based on exit status.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

run_test_runner "filter/0002_filter_sample" || {
    echo "not ok 1 - 0002_filter_sample"
    exit 0
}

echo "ok 1 - 0002_filter_sample"
exit 0
