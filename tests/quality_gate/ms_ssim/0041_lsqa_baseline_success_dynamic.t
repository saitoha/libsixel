#!/bin/sh
# Verify that a dynamic baseline below the measured MS-SSIM returns success.

set -eux


printf '1..1\n'
set -v

image_ref="${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp"
image_out="${TOP_SRCDIR}/tests/data/inputs/snake_64.six"
metric=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM "${image_ref}" "${image_out}") || {
    echo "not ok" 1 - "failed to calculate MS-SSIM"
    exit 0
}
baseline=$(printf '%s\n' "${metric}" |
    awk 'BEGIN{b=0} {b=$1-0.0001; if (b < 0) b=0; printf "%.6f", b}')

set +e
${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -b "MS-SSIM:${baseline}" \
    "${image_ref}" "${image_out}" >/dev/null 2>&1
status=$?
set -e

test "${status}" -eq 0 || {
    echo "not ok" 1 - "baseline below metric should return success"
    exit 0
}

echo "ok" 1 - "baseline below metric returned success"

exit 0
