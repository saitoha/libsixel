#!/bin/sh
# Verify packet empty mask.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0075_packet_empty_mask" || {
    printf '\n%s\n' "not ok 1 - packet empty mask"
    exit 0
}
printf '\n%s\n' "ok 1 - packet empty mask"
exit 0
