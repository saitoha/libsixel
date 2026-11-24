#!/bin/sh
# Verify map_pixel_hint() returns map_pixel()'s index for every lookup policy.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "lookup/0014_hint_contract" || {
    printf '\n%s\n' "not ok 1 - lookup hint contract"
    exit 0
}

printf '\n%s\n' "ok 1 - lookup hint contract"
exit 0
