#!/bin/sh
# A short IEND CRC write reports an error and releases the output and buffers.
set -eux

echo "1..1"
set -v

SIXEL_LOG_PATH='' ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    writer/png/0067_iend_crc_short_write_cleanup || {
    echo "not ok 1 - short IEND CRC write reports an error and releases resources"
    exit 0
}
echo "ok 1 - short IEND CRC write reports an error and releases resources"
exit 0
