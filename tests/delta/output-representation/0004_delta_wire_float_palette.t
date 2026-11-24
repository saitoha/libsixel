#!/bin/sh
# Verify delta wire float palette.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0054_delta_wire_float_palette || {
    printf '\n%s\n' "not ok 1 - delta wire float palette"
    exit 0
}
printf '\n%s\n' "ok 1 - delta wire float palette"
exit 0
