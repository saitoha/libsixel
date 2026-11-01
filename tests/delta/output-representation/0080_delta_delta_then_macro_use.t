#!/bin/sh
# Selecting automatic macro output after delta rejects before changing selection.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0223_delta_delta_then_macro_use || {
    echo "not ok 1 - delta then macro use"
    exit 0
}
echo "ok 1 - delta then macro use"
exit 0
