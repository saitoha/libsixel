#!/bin/sh
# TAP test covering float32 palette lookup without LUT caching.
#
# Flow:
# - Convert the 64x64 snake reference with float32 precision and no LUT.
# - Run lsqa to ensure quality stays above the minimum floor.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
. "${TOP_SRCDIR}/tests/lib/sh/common/tap.sh"
. "${TOP_SRCDIR}/tests/lib/sh/lsqa/lsqa_common.sh"

status=0

lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.98}

config_macro_defined HAVE_IMG2SIXEL || skip_all

echo "1..1"
set -v

input_image="${top_srcdir}/tests/data/inputs/snake_64.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/output.six"

if run_img2sixel --precision=float32 --lookup-policy=none -o "${output_sixel}" "${input_image}"; then
    :
else
    fail 1 "float32 lookup policy none lsqa failed"
    exit "${status}"
fi

lsqa_err=$(
    set +xv
    run_lsqa -b "MS-SSIM:${lsqa_floor}" "${input_image}" \
        "${output_sixel}" 2>&1
) || lsqa_run_status=$?

if [ -z "${lsqa_run_status-}" ]; then
    pass 1 "float32 lookup policy none lsqa passed"
elif [ "${lsqa_run_status}" -eq 5 ]; then
    fail 1 "${lsqa_err}"
else
    fail 1 "float32 lookup policy none lsqa failed"
fi

exit "${status}"
