#!/bin/sh
# Verify default soft mask p3 with visible-only repair and an omission key.
# Policy: docs/encoder/palette-construction/cover-policy/README.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    quant/palette/cover_mask/0001_cover_mask wire 14 || {
    echo "not ok 1 - default soft mask p3"
    exit 0
}

echo "ok 1 - default soft mask p3"
exit 0
