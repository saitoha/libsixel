#!/bin/sh
# Selecting DRCS after delta rejects before glyph settings or tile routing change.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0215_delta_delta_then_drcs || {
    echo "not ok 1 - delta then drcs"
    exit 0
}
echo "ok 1 - delta then drcs"
exit 0
