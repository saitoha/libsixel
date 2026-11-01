#!/bin/sh
# Verify the deliberately wrong baseline fails the quality gate.
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

status=0
score=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -M 6delta-progressive \
    -m MS-SSIM -b MS-SSIM:0.98 "${TOP_SRCDIR}/tests/data/inputs/snake_192.png" \
    "${TOP_SRCDIR}/tests/data/inputs/progressive-snake-192-baseline.six" \
    ) || status=$?
test "${status}" -eq 5 || {
    echo "not ok 1 - incorrect baseline did not fail quality assessment"
    exit 0
}

# A NaN rejection is not evidence that the black baseline was measured.
case "${score}" in
    0.[0-9]*) ;;
    *)
        echo "not ok 1 - baseline comparison must produce a finite score"
        exit 0
        ;;
esac
test "${score#0.}" -ge 0 2>/dev/null || {
    echo "not ok 1 - baseline score contains unexpected output"
    exit 0
}

echo "ok 1 - multi pass baseline control"
exit 0
