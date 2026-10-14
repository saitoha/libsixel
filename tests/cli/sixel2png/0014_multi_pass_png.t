#!/bin/sh
# Verify the final PNG includes the correcting pass.
# Policy: docs/decoder/multi-pass.md

# The capture and independent reference both retain five SSIM scales.
set -eux

test "${HAVE_SIXEL2PNG-}" = 1 || {
    echo "1..0 # SKIP sixel2png is disabled"
    exit 0
}

test -n "${LSQA_PATH-}" && test -x "${LSQA_PATH}" || {
    echo "1..0 # SKIP lsqa is disabled"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" -M 6delta-progressive \
    -i "${TOP_SRCDIR}/tests/data/inputs/progressive-snake-192.six" | \
    ${SIXEL_RUNTIME-} "${LSQA_PATH}" -b MS-SSIM:0.98 \
    "${TOP_SRCDIR}/tests/data/inputs/snake_192.png" - >/dev/null || {
    echo "not ok 1 - final PNG differs from reference"
    exit 0
}

echo "ok 1 - multi pass png"
exit 0
