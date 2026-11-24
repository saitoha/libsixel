#!/bin/sh
# Verify extended srgb encode against independent fixtures.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "colorspace/0005_extended_srgb_encode" || {
    printf '\n%s\n' "not ok 1 - extended srgb encode"
    exit 0
}

printf '\n%s\n' "ok 1 - extended srgb encode"
exit 0
