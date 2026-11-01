#!/bin/sh
# Verify packet unused palette.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0071_packet_unused_palette" || {
    echo "not ok 1 - packet unused palette"
    exit 0
}
echo "ok 1 - packet unused palette"
exit 0
