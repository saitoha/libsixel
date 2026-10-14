#!/bin/sh
# Verify MS-SSIM aliases return identical single-metric output.

# The five-scale metric requires at least 176 pixels in both dimensions.
set -eux


printf '1..1\n'
set -v

image_ref="${TOP_SRCDIR}/images/snake.png"
image_out="${TOP_SRCDIR}/images/snake.six"
value_canonical=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM "${image_ref}" "${image_out}") || {
    echo "not ok" 1 - "failed to calculate canonical MS-SSIM"
    exit 0
}
value_alias1=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS_SSIM "${image_ref}" "${image_out}") || {
    echo "not ok" 1 - "failed to calculate MS_SSIM alias"
    exit 0
}
value_alias2=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -m SSIM "${image_ref}" "${image_out}") || {
    echo "not ok" 1 - "failed to calculate SSIM alias"
    exit 0
}

test "${value_canonical}" = "${value_alias1}" || {
    echo "not ok" 1 - "MS-SSIM canonical and MS_SSIM differ"
    exit 0
}

test "${value_canonical}" = "${value_alias2}" || {
    echo "not ok" 1 - "MS-SSIM canonical and SSIM differ"
    exit 0
}

echo "ok" 1 - "MS-SSIM aliases are equivalent"
exit 0
