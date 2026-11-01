#!/bin/sh
# Verify soft opaque ceiling.
# Policy: docs/encoder/palette-construction/cover-policy/README.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    quant/palette/cover_mask/0001_cover_mask unit 256 255 1 0 || {
    echo "not ok 1 - soft opaque ceiling"
    exit 0
}

echo "ok 1 - soft opaque ceiling"
exit 0
