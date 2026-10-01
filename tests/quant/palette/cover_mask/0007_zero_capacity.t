#!/bin/sh
# Verify zero capacity.
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    quant/palette/cover_mask/0001_cover_mask unit 0 254 1 0 || {
    echo "not ok 1 - zero capacity"
    exit 0
}

echo "ok 1 - zero capacity"
exit 0
