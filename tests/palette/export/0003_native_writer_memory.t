#!/bin/sh
# Verify palette export native writer memory.
# Policy: docs/encoder/palette-construction/external-palettes.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/export_capture/0003_native_writer_memory "${TOP_SRCDIR}/tests/data/inputs/snake_64.ppm" || {
    echo "not ok 1 - palette export native writer memory"
    exit 0
}
echo "ok 1 - palette export native writer memory"
exit 0
