#!/bin/sh
# Every decimal entry preserves bytes and the five-byte reservation.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/pipeline/0128_count_table" || {
    echo "not ok 1 - decimal table bytes and bounds"
    exit 0
}
echo "ok 1 - decimal table bytes and bounds"
exit 0
