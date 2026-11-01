#!/bin/sh
# Verify clear map sparse serial.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "encoder-core/0059_clear_map_sparse_serial" || {
    echo "not ok 1 - clear map sparse serial"
    exit 0
}

echo "ok 1 - clear map sparse serial"
exit 0
