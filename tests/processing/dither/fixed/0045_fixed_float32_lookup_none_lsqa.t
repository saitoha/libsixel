#!/bin/sh
# TAP test covering float32 palette lookup without LUT caching.
#
# Flow:
# - Convert the 64x64 snake reference with float32 precision and no LUT.
# - Run lsqa to ensure quality stays above the minimum floor.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.98}

echo "1..1"
set -v

input_image="${top_srcdir}/tests/data/inputs/snake_64.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/output.six"

run_img2sixel --precision=float32 --lookup-policy=none -o "${output_sixel}" "${input_image}" || {
    fail 1 "float32 lookup policy none lsqa failed"
    exit 0
}

lsqa_err=$(
    set +xv
    run_lsqa -b "MS-SSIM:${lsqa_floor}" "${input_image}" \
        "${output_sixel}" 2>&1
) || lsqa_run_status=$?

[ "${lsqa_run_status:-0}" -eq 0 ] && {
    pass 1 "float32 lookup policy none lsqa passed"
    exit 0
}

[ "${lsqa_run_status}" -eq 5 ] && {
    fail 1 "${lsqa_err}"
    exit 0
}

fail 1 "float32 lookup policy none lsqa failed"

exit 0
