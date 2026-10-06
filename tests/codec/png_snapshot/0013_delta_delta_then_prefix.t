#!/bin/sh
# Selecting a prefixed PNG path after delta leaves destination ownership unchanged.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    codec/png_snapshot/0013_delta_delta_then_prefix || {
    echo "not ok 1 - PNG delta delta then prefix"
    exit 0
}
echo "ok 1 - PNG delta delta then prefix"
exit 0
