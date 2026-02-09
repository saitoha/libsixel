#!/bin/sh
# Verify that lsqa exits with code 5 when baseline is above measured value.

set -eu

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

printf '1..1\n'
set -v

image_ref="${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp"
image_out="${TOP_SRCDIR}/tests/data/inputs/snake_64.six"
metric=$(run_lsqa -m MS-SSIM "${image_ref}" "${image_out}") || {
    fail 1 "failed to calculate MS-SSIM"
    exit 0
}
baseline=$(printf '%s\n' "${metric}" |
    awk 'BEGIN{b=0} {b=$1+0.0001; if (b > 1.1) b=1.1; printf "%.6f", b}')

run_lsqa -m MS-SSIM -b "MS-SSIM:${baseline}" \
    "${image_ref}" "${image_out}" >/dev/null 2>&1 || status=$?
if [ "${status-0}" -eq 5 ]; then
    pass 1 "baseline above metric returned exit code 5"
else
    fail 1 "expected exit code 5, got ${status-0}"
fi

exit 0
