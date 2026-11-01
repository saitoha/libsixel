#!/bin/sh
# Verify native soft cover: heckbert p3 wire.
# Policy: docs/encoder/palette-construction/cover-policy/README.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/cover_float/0001_cover_contract wire 3 0 1 || {
    echo "not ok 1 - heckbert p3 wire"
    exit 0
}

echo "ok 1 - heckbert p3 wire"
exit 0
