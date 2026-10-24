#!/bin/sh
# Policy: docs/encoder/output/output-colorspace.md
# Verify packed alpha: layout abgr.
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_THREADING_GLOBAL_BUDGET=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    quant/palette/packed_alpha/0004_layout_abgr || {
    echo "not ok 1 - packed alpha layout abgr"
    exit 0
}

echo "ok 1 - packed alpha layout abgr"
exit 0
