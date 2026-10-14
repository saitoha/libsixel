#!/bin/sh
# Check palette PNG stability across repeated lsqa runs.

# The five-scale metric requires at least 176 pixels in both dimensions.
set -eux


printf '1..1\n'
set -v

image1="${TOP_SRCDIR}/images/snake-palette.png"
image2="${TOP_SRCDIR}/images/snake.six"

value0=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM "${image1}" "${image2}")
value1=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM "${image1}" "${image2}")
value2=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM "${image1}" "${image2}")

test "${value0}" = "${value1}" || {
    echo "not ok" 1 - "palette repeat variance exceeded tolerance"
    exit 0
}

test "${value1}" = "${value2}" || {
    echo "not ok" 1 - "palette repeat variance exceeded tolerance"
    exit 0
}

echo "ok" 1 - "palette repeat variance within tolerance"
exit 0
