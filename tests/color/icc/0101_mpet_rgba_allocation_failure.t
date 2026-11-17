#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0101_mpet_rgba_allocation_failure || {
    echo "not ok 1 - ordered RGBA8 propagates allocation failure"
    exit 0
}

echo "ok 1 - ordered RGBA8 propagates allocation failure"
exit 0
