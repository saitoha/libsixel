#!/bin/sh
# Verify extended oklab inverse against independent fixtures.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "colorspace/0007_extended_oklab_inverse" || {
    printf '\n%s\n' "not ok 1 - extended oklab inverse"
    exit 0
}

printf '\n%s\n' "ok 1 - extended oklab inverse"
exit 0
