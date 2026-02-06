# The lsqa helper can read SIXEL directly, so compare with SIXEL output.
# Quality floors tuned to requested QA thresholds:
# - MS-SSIM floor: 0.97
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.98}

echo "1..1"
set -v

input_image="${top_srcdir}/tests/data/inputs/snake_64.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/vpte-float32-din99d.six"

if run_img2sixel --lookup-policy=vpte --precision=float32 \
        --working-colorspace=din99d -o "${output_sixel}" \
    "${input_image}" \
; then
    :
else
    fail 1 "float32 VPTE din99d colorspace conversion failed"
    exit 0
fi

lsqa_err=$(
    set +xv
    run_lsqa -b "MS-SSIM:${lsqa_floor}" "${input_image}" "${output_sixel}" 2>&1
) || lsqa_run_status=$?

if [ -z "${lsqa_run_status-}" ]; then
    pass 1 "float32 VPTE din99d colorspace lsqa passed"
elif [ "${lsqa_run_status}" -eq 5 ]; then
    fail 1 "${lsqa_err}"
else
    fail 1 "float32 VPTE din99d colorspace lsqa failed"
fi

exit 0
