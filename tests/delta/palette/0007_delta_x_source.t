#!/bin/sh
# Verify delta x source.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0019_delta_x_source || {
    echo "not ok 1 - delta x source"
    exit 0
}
echo "ok 1 - delta x source"
exit 0
