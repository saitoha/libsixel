#!/bin/sh
# Verify palette keep cost.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0003_palette_keep_cost" || {
    printf '\n%s\n' "not ok 1 - palette keep cost"
    exit 0
}
printf '\n%s\n' "ok 1 - palette keep cost"
exit 0
