#!/bin/sh
# Verify delta source mask.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0004_delta_source_mask || {
    echo "not ok 1 - delta source mask"
    exit 0
}
echo "ok 1 - delta source mask"
exit 0
