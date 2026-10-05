#!/bin/sh
# Verify delta blue byte.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0029_delta_blue_byte || {
    echo "not ok 1 - delta blue byte"
    exit 0
}
echo "ok 1 - delta blue byte"
exit 0
