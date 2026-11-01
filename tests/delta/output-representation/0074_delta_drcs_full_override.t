#!/bin/sh
# Selecting full before DRCS clears delta history and permits glyph definitions.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0217_delta_drcs_full_override || {
    echo "not ok 1 - drcs full override"
    exit 0
}
echo "ok 1 - drcs full override"
exit 0
