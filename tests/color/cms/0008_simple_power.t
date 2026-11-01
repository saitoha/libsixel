#!/bin/sh
# Verify simple power.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "cms/0002_simple_power" || {
    echo "not ok 1 - simple power"
    exit 0
}

echo "ok 1 - simple power"
exit 0
