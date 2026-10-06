#!/bin/sh
# Switching a PNG destination back to ordinary SIXEL permits delta.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    codec/png_snapshot/0018_delta_sixel_switch || {
    echo "not ok 1 - PNG delta sixel switch"
    exit 0
}
echo "ok 1 - PNG delta sixel switch"
exit 0
