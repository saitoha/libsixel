#!/bin/sh
# Verify extended srgb decode against independent fixtures.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "colorspace/0004_extended_srgb_decode" || {
    echo "not ok 1 - extended srgb decode"
    exit 0
}

echo "ok 1 - extended srgb decode"
exit 0
