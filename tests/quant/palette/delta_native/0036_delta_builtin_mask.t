#!/bin/sh
# Verify delta builtin mask.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0036_delta_builtin_mask || {
    echo "not ok 1 - delta builtin mask"
    exit 0
}
echo "ok 1 - delta builtin mask"
exit 0
