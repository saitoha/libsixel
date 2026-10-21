#!/bin/sh
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

${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" -M6 \
    -i "${TOP_SRCDIR}/tests/data/inputs/progressive-snake-192.six" | \
    ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -b MS-SSIM:0.98 \
    "${TOP_SRCDIR}/tests/data/inputs/snake_192.png" - >/dev/null || {
    echo "not ok 1 - -M6 did not compose the final PNG"
    exit 0
}

${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" --input-mode=6delta-pro \
    -i "${TOP_SRCDIR}/tests/data/inputs/progressive-snake-192.six" | \
    ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -b MS-SSIM:0.98 \
    "${TOP_SRCDIR}/tests/data/inputs/snake_192.png" - >/dev/null || {
    echo "not ok 1 - --input-mode=6delta-pro did not compose the final PNG"
    exit 0
}

echo "ok 1 - input mode prefixes compose the final PNG"
exit 0
