#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0050_absolute_unpaired || {
    echo "not ok 1 - an unavailable absolute path leaves a complete saturation pair"
    exit 0
}

echo "ok 1 - an unavailable absolute path leaves a complete saturation pair"
exit 0
