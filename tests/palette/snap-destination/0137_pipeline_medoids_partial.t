#!/bin/sh
# Verify destination snap: pipeline medoids partial.
# Policy: docs/encoder/palette-construction/snap-policy/README.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/p3_snap/0002_pipeline 2 2 0 1 || {
    echo "not ok 1 - P3 snap pipeline medoids partial"
    exit 0
}

echo "ok 1 - P3 snap pipeline medoids partial"
exit 0
