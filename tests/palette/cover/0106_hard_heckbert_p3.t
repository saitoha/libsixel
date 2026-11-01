#!/bin/sh
# Verify native cover: hard heckbert p3.
# Policy: docs/encoder/palette-construction/cover-policy/README.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/cover_float/0001_cover_contract wire 3 0 3 || {
    echo "not ok 1 - hard heckbert p3"
    exit 0
}

echo "ok 1 - hard heckbert p3"
exit 0
