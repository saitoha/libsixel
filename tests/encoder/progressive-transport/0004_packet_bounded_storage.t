#!/bin/sh
# Verify packet bounded storage.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0072_packet_bounded_storage" || {
    printf '\n%s\n' "not ok 1 - packet bounded storage"
    exit 0
}
printf '\n%s\n' "ok 1 - packet bounded storage"
exit 0
