#!/bin/sh
# Verify resize applies to the final composed image.
# Policy: docs/decoder/multi-pass.md

set -eux

test "${HAVE_SIXEL2PNG-}" = 1 || {
    echo "1..0 # SKIP sixel2png is disabled"
    exit 0
}

echo "1..1"
set -v

dimensions=$(${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" \
    --input-mode=6delta-progressive -s 8 \
    -i "${TOP_SRCDIR}/tests/data/inputs/progressive-snake-16.six" | \
    od -An -tx1 -j16 -N8) || {
    echo "not ok 1 - final image resize failed"
    exit 0
}
# shellcheck disable=SC2086
set -- ${dimensions}
test "$*" = "00 00 00 08 00 00 00 08" || {
    echo "not ok 1 - unexpected final PNG dimensions"
    exit 0
}

echo "ok 1 - multi pass resize"
exit 0
