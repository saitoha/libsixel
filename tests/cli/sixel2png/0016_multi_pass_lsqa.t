#!/bin/sh
# Verify explicit multi-pass target assessment and numeric output.
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

score=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    --input-mode=6delta-progressive -m MS-SSIM -b MS-SSIM:0.98 \
    "${TOP_SRCDIR}/tests/data/inputs/snake_192.png" \
    "${TOP_SRCDIR}/tests/data/inputs/progressive-snake-192.six") || {
    echo "not ok 1 - final capture quality failed"
    exit 0
}
case "${score}" in
    0.[0-9]*) ;;
    *)
        echo "not ok 1 - metric output contains capture diagnostics"
        exit 0
        ;;
esac
test "${score#0.}" -ge 0 2>/dev/null || {
    echo "not ok 1 - metric output contains capture diagnostics"
    exit 0
}

echo "ok 1 - multi pass lsqa"
exit 0
