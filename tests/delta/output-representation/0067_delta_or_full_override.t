#!/bin/sh
# Selecting full before OR clears delta history and permits OR output.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0210_delta_or_full_override || {
    echo "not ok 1 - or full override"
    exit 0
}
echo "ok 1 - or full override"
exit 0
