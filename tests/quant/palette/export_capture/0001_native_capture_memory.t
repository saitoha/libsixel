#!/bin/sh
# Verify palette export native capture memory.
# Policy: docs/encoder/palette-construction/external-palettes.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/export_capture/0001_native_capture_memory || {
    echo "not ok 1 - palette export native capture memory"
    exit 0
}
echo "ok 1 - palette export native capture memory"
exit 0
