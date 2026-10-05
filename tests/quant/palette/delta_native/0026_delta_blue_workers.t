#!/bin/sh
# Verify delta blue workers.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0026_delta_blue_workers || {
    echo "not ok 1 - delta blue workers"
    exit 0
}
echo "ok 1 - delta blue workers"
exit 0
