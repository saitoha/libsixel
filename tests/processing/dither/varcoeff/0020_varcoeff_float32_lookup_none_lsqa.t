#!/bin/sh
# TAP test covering float32 variable-coefficient dither with lookup none.
#
# Flow:
# - Convert the snake reference with float32 precision and lso2 diffusion.
# - Disable LUT lookup so dither-common-pipeline float32 palette scan runs.
# - Verify quality with lsqa so the conversion remains visually stable.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

# Threshold rationale:
# - lookup-policy=none forces the float32 full scan path that this case wants
#   to validate.
# - lso2 varcoeff diffusion is more stable than fs under the same palette size,
#   so 0.96 is strict enough to catch regressions without becoming flaky.
lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.96}

echo "1..1"
set -v

input_image="${top_srcdir}/tests/data/inputs/snake_64.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/varcoeff-float32-lookup-none.six"

if run_img2sixel -d lso2 -y serpentine --precision=float32 \
        --lookup-policy=none -p 16 -o "${output_sixel}" "${input_image}"; then
    :
else
    fail 1 "varcoeff float32 lookup none conversion failed"
    exit 0
fi

lsqa_err=$(
    set +xv
    run_lsqa -b "MS-SSIM:${lsqa_floor}" "${input_image}" \
        "${output_sixel}" 2>&1
) || lsqa_run_status=$?

if [ -z "${lsqa_run_status-}" ]; then
    pass 1 "varcoeff float32 lookup none lsqa passed"
elif [ "${lsqa_run_status}" -eq 5 ]; then
    fail 1 "${lsqa_err}"
else
    fail 1 "varcoeff float32 lookup none lsqa failed"
fi

exit 0
