#!/bin/sh
# Verify extended lab forward against independent fixtures.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "colorspace/0008_extended_lab_forward" || {
    printf '\n%s\n' "not ok 1 - extended lab forward"
    exit 0
}

printf '\n%s\n' "ok 1 - extended lab forward"
exit 0
