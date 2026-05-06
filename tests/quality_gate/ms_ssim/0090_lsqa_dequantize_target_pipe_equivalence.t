#!/bin/sh
# Verify lsqa target dequantize matches the sixel2png pipe form.

set -eux

test -n "${SIXEL2PNG_PATH-}" || {
    printf '1..0 # SKIP sixel2png is disabled in this build\n'
    exit 0
}
test -n "${LSQA_PATH-}" || {
    printf '1..0 # SKIP lsqa is disabled in this build\n'
    exit 0
}

printf '1..1\n'
set -v

image_ref="${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp"
image_six="${TOP_SRCDIR}/tests/data/inputs/snake_64.six"

direct_value=$(
    ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM \
        "${image_ref}" -dk_undither -S120 -e25 <"${image_six}"
) || {
    echo "not ok" 1 - "lsqa target dequantize failed"
    exit 0
}

pipe_value=$(
    ${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" -dk_undither -S120 -e25 \
        -i "${image_six}" -o - |
    ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM "${image_ref}" -
) || {
    echo "not ok" 1 - "sixel2png dequantize pipe failed"
    exit 0
}

test "${direct_value}" = "${pipe_value}" || {
    echo "not ok" 1 - "lsqa dequantize did not match sixel2png pipe"
    exit 0
}

echo "ok" 1 - "lsqa target dequantize matches sixel2png pipe"
exit 0
