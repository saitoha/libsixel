#!/bin/sh
# Verify extended srgb encode against independent fixtures.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "colorspace/0005_extended_srgb_encode" || {
    echo "not ok 1 - extended srgb encode"
    exit 0
}

echo "ok 1 - extended srgb encode"
exit 0
