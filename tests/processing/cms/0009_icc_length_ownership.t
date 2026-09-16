#!/bin/sh
# Verify icc length ownership.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "cms/0009_icc_length_ownership" || {
    echo "not ok 1 - icc length ownership"
    exit 0
}

echo "ok 1 - icc length ownership"
exit 0
