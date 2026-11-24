#!/bin/sh
# Verify palette swap.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0005_palette_swap" || {
    printf '\n%s\n' "not ok 1 - palette swap"
    exit 0
}
printf '\n%s\n' "ok 1 - palette swap"
exit 0
