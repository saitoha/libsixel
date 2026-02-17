#!/bin/sh
# Verify WIC DDS DXT3 decoding quality with an MS-SSIM baseline.
# Reproduction command (ImageMagick):
#   convert tests/data/inputs/formats/snake-64-reference-rgba.png -define dds:compression=dxt3 DDS:tests/data/inputs/formats/snake-dds-dxt3.dds

set -eu

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"
feature_defined_in_config "HAVE_WIC" || skip_all "wic loader is unavailable"

image_path="${TOP_SRCDIR}/tests/data/inputs/formats/snake-dds-dxt3.dds"

set +e
probe_output=$(run_img2sixel -Lwic! "${image_path}" -o/dev/null 2>&1)
probe_status=$?
set -e

printf '%s' "${probe_output}" |
grep -q "{cacaf262-9370-4615-a13b-9f5539da4c0a} not registered" && skip_all "WIC is not available"

test "${probe_status}" -eq 0 || skip_all "wic dds dxt3 codec is unavailable"

lsqa_floor=${LSQA_MS_SSIM_FLOOR_WIC_DDS_DXT3:-0.95}

printf '1..1
'
set -v

reference_path="${TOP_SRCDIR}/tests/data/inputs/formats/snake-64-reference-rgb.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/wic_dds_dxt3.six"
run_img2sixel -Lwic! "${image_path}" >"${output_sixel}" || {
    fail 1 "wic dds dxt3 conversion failed"
    exit 0
}

lsqa_err=$(
    run_lsqa -b "MS-SSIM:${lsqa_floor}" "${reference_path}" "${output_sixel}" 2>&1
) || lsqa_run_status=$?

test "${lsqa_run_status:-0}" -eq 0 && {
    pass 1 "wic dds dxt3 quality meets baseline"
    exit 0
}

test "${lsqa_run_status}" -eq 5 && {
    fail 1 "${lsqa_err}"
    exit 0
}

fail 1 "wic dds dxt3 quality regressed"
exit 0
