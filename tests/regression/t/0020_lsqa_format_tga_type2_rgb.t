#!/bin/sh
# Verify TGA type 2 (uncompressed RGB) quality against lsqa baselines.

set -eu

if [ "${VERBOSE:-0}" -eq 1 ]; then
    set -x
fi

test_name=$(basename "$0")
test_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
category_name=$(basename "$(dirname "${test_dir}")")


lsqa_common_path="${test_dir}/../../lib/sh/lsqa/lsqa_common.sh"
LSQA_HELPER_DIR=$(CDPATH=; cd "$(dirname "${lsqa_common_path}")" && pwd)
export LSQA_HELPER_DIR
. "${lsqa_common_path}"

status=0

lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.98}

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"


artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${category_name}/${test_name}"
mkdir -p "${artifact_dir}"

printf '1..1\n'
set -v

image_path="${top_srcdir}/tests/data/inputs/formats/snake-tga-type2-rgb.tga"
output_sixel="${artifact_dir}/output.six"
if run_img2sixel -Lbuiltin "${image_path}" >"${output_sixel}" && \
    {
        lsqa_err_file=$(mktemp)
        lsqa_run_status=0
        if ! run_lsqa -b "MS-SSIM:${lsqa_floor}" \
            "${image_path}" "${output_sixel}" > /dev/null \
            2>"${lsqa_err_file}"; then
            lsqa_run_status=$?
            printf '# %s: assessment/lsqa returned %s\n' \
                "snake-tga-type2-rgb.tga" "${lsqa_run_status}"
            if [ -s "${lsqa_err_file}" ]; then
                printf '# lsqa stderr follows\n'
                sed 's/^/# /' "${lsqa_err_file}"
            else
                printf '# %s: lsqa produced no diagnostics\n' \
                    "snake-tga-type2-rgb.tga"
            fi
        fi
        rm -f "${lsqa_err_file}"
        [ ${lsqa_run_status} -eq 0 ]; }; then
    pass 1 "type 2 RGB TGA meets lsqa floor"
else
    fail 1 "type 2 RGB TGA quality below floor"
fi

exit "${status}"
