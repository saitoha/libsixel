#!/bin/sh
# Verify WIC DDS DXT3 decoding quality with an MS-SSIM baseline.
# Reproduction command (ImageMagick):
#   convert tests/data/inputs/formats/snake-64-reference-rgba.png -define dds:compression=dxt3 DDS:tests/data/inputs/formats/snake-dds-dxt3.dds

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}
test "${HAVE_WIC-}" = 1 || {
    printf "1..0 # SKIP wic loader is unavailable\n";
    exit 0
}
test "${RUNTIME_ENV_IS_WINE-0}" -eq 1 && {
    printf "1..0 # SKIP WIC is unavailable under wine\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

printf '1..1\n'
set -v

lsqa_floor=0.95

image_path="${TOP_SRCDIR}/tests/data/inputs/formats/snake-dds-dxt3.dds"
reference_path="${TOP_SRCDIR}/tests/data/inputs/formats/snake-64-reference-rgb.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/wic_dds_dxt3.six"
run_img2sixel -Lwic! "${image_path}" >"${output_sixel}" || {
    echo "not ok" 1 "wic dds dxt3 conversion failed"
    exit 0
}

lsqa_err=$(
    run_lsqa -b "MS-SSIM:${lsqa_floor}" "${reference_path}" "${output_sixel}" 2>&1
) || lsqa_run_status=$?

test "${lsqa_run_status:-0}" -eq 0 && {
    echo "ok" 1 "wic dds dxt3 quality meets baseline"
    exit 0
}

test "${lsqa_run_status}" -eq 5 && {
    echo "not ok" 1 "${lsqa_err}"
    exit 0
}

echo "not ok" 1 "wic dds dxt3 quality regressed"
exit 0
