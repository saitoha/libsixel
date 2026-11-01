#!/bin/sh
# Verify exact binning executes through the shared filter vtable.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "filter/0033_filter_binning_exact" || {
    echo "not ok 1 - exact binning filter contract"
    exit 0
}

echo "ok 1 - exact binning filter contract"
exit 0
