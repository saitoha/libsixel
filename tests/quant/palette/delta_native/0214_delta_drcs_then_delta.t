#!/bin/sh
# Selecting delta after DRCS rejects without changing options or retained history.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0214_delta_drcs_then_delta || {
    echo "not ok 1 - drcs then delta"
    exit 0
}
echo "ok 1 - drcs then delta"
exit 0
