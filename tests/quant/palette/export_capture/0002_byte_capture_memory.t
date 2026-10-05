#!/bin/sh
# Verify palette export byte capture memory.
# Policy: docs/encoder/palette-construction/external-palettes.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/export_capture/0002_byte_capture_memory || {
    echo "not ok 1 - palette export byte capture memory"
    exit 0
}
echo "ok 1 - palette export byte capture memory"
exit 0
