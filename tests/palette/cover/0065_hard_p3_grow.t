#!/bin/sh
# Verify native cover: hard p3 grow.
# Policy: docs/encoder/palette-construction/cover-policy/README.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/cover_float/0001_cover_contract wire 0 0 3 || {
    echo "not ok 1 - hard p3 grow"
    exit 0
}

echo "ok 1 - hard p3 grow"
exit 0
