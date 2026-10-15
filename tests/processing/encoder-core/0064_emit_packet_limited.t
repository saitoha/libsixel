#!/bin/sh
# Verify emit packet limited stream bytes.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "encoder-core/0064_emit_packet_limited" || {
    echo "not ok 1 - emit packet limited stream bytes"
    exit 0
}

echo "ok 1 - emit packet limited stream bytes"
exit 0
