#!/bin/sh
# Verify hard reserved ceiling.
# Policy: docs/encoder/palette-construction/cover-policy/README.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    quant/palette/cover_mask/0001_cover_mask unit 255 254 1 1 || {
    echo "not ok 1 - hard reserved ceiling"
    exit 0
}

echo "ok 1 - hard reserved ceiling"
exit 0
