#!/bin/sh
# Verify delta builtin slots.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0033_delta_builtin_slots || {
    echo "not ok 1 - delta builtin slots"
    exit 0
}
echo "ok 1 - delta builtin slots"
exit 0
