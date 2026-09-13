#!/bin/sh
# Verify extended oklab forward against independent fixtures.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "colorspace/0006_extended_oklab_forward" || {
    echo "not ok 1 - extended oklab forward"
    exit 0
}

echo "ok 1 - extended oklab forward"
exit 0
