#!/bin/sh
# The high-level frame entry preserves subbyte samples through high-color output.
# Policy: docs/encoder/output/high-color/README.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" codec/highcolor/0010_frame_float || {
    echo "not ok 1 - high-color frame float"
    exit 0
}
echo "ok 1 - high-color frame float"
exit 0
