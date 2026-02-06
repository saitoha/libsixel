#!/bin/sh
# Run lsqa checks for float32 VPTE in the oklab colorspace.
# The lsqa helper can read SIXEL directly, so compare with SIXEL output.
# Quality floors tuned to requested QA thresholds:
# - MS-SSIM floor: 0.96
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
output_sixel="${ARTIFACT_LOCAL_DIR}/vpte-float32-oklab.six"

if run_img2sixel --lookup-policy=vpte --precision=float32 \
        --working-colorspace=oklab -o "${output_sixel}" \
    "${input_image}" \
; then
    :
else
    fail 1 "float32 VPTE oklab colorspace conversion failed"
    exit "${status}"
fi

lsqa_err=$(
    set +xv
    run_lsqa -b "MS-SSIM:${lsqa_floor}" "${input_image}" "${output_sixel}" 2>&1
) || lsqa_run_status=$?

if [ -z "${lsqa_run_status-}" ]; then
    pass 1 "float32 VPTE oklab colorspace lsqa passed"
elif [ "${lsqa_run_status}" -eq 5 ]; then
    fail 1 "${lsqa_err}"
else
    fail 1 "float32 VPTE oklab colorspace lsqa failed"
fi

exit "${status}"
