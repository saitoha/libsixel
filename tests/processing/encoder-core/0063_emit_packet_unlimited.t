#!/bin/sh
# Verify emit packet unlimited stream bytes.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "encoder-core/0063_emit_packet_unlimited" || {
    echo "not ok 1 - emit packet unlimited stream bytes"
    exit 0
}

echo "ok 1 - emit packet unlimited stream bytes"
exit 0
