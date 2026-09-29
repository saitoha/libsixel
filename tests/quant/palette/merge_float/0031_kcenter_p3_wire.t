#!/bin/sh
# Verify kcenter p3 wire.
# Policy: docs/encoder/palette-construction/merge-policy/README.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/merge_float/0001_merge_contract wire 1 1 1 || {
    echo "not ok 1 - kcenter p3 wire"
    exit 0
}

echo "ok 1 - kcenter p3 wire"
exit 0
