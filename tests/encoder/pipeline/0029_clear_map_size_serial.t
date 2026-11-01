#!/bin/sh
# Verify clear map size serial.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "encoder-core/0057_clear_map_size_serial" || {
    echo "not ok 1 - clear map size serial"
    exit 0
}

echo "ok 1 - clear map size serial"
exit 0
