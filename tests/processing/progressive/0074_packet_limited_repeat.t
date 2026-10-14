#!/bin/sh
# Verify packet limited repeat.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0074_packet_limited_repeat" || {
    echo "not ok 1 - packet limited repeat"
    exit 0
}
echo "ok 1 - packet limited repeat"
exit 0
