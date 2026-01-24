#!/bin/sh
# Run lsqa quality checks for Heckbert final merge with 8-bit palettes.
set -eux

conversion_common_path=$(CDPATH=; cd "$(dirname "$0")/.." && pwd)/../lib/sh/conversion/common.sh
. "${conversion_common_path}"

palette_lsqa_path=$(CDPATH=; cd "$(dirname "$0")/.." && pwd)/../lib/sh/lsqa/lsqa_palette_common.sh
PALETTE_LSQA_HELPER_DIR=$(CDPATH=; cd "$(dirname "${palette_lsqa_path}")" && pwd)
export PALETTE_LSQA_HELPER_DIR
. "${palette_lsqa_path}"

test_name=$(basename "$0")
setup_conversion_env "${test_name}"

status=0

ensure_img2sixel_available
ensure_converter_available "SIXEL2PNG" "${SIXEL2PNG_PATH}" "sixel2png"

echo "1..1"

input_image="${top_srcdir}/tests/data/resolutions/tiny_square.png"
output_sixel="${output_dir}/merge-heckbert-8bit.six"
output_png="${output_dir}/merge-heckbert-8bit.png"

require_file "${input_image}"

if ! palette_lsqa_init "$0"; then
    fail 1 "lsqa binary missing"
    exit "${status}"
fi

if SIXEL_PALETTE_OVERSPLIT_FACTOR=2.2 \
        SIXEL_PALETTE_FINAL_MERGE_ADDITIONAL_LLOYD_ITER_COUNT=2 \
        SIXEL_PALETTE_KMEANS_ITER_COUNT_MAX=5 \
        SIXEL_PALETTE_KMEANS_THRESHOLD=0.1 \
        SIXEL_PALETTE_LUMIN_FACTOR_R=0.3 \
        SIXEL_PALETTE_LUMIN_FACTOR_G=0.4 \
        SIXEL_PALETTE_MERGE_CHANNEL_FACTOR_L=0.6 \
        run_img2sixel -Q heckbert -F ward \
        -o "${output_sixel}" "${input_image}" 2>>"${log_file}"; then
    :
else
    fail 1 "img2sixel merge heckbert 8bit failed"
    exit "${status}"
fi

if run_sixel2png -i "${output_sixel}" -o "${output_png}" \
        2>>"${log_file}"; then
    :
else
    fail 1 "sixel2png decode failed"
    exit "${status}"
fi

if palette_lsqa_assert_quality "${input_image}" "${output_png}" \
        "merge-heckbert-8bit" "${artifact_dir}"; then
    pass 1 "merge heckbert 8bit lsqa passed"
else
    fail 1 "merge heckbert 8bit lsqa failed"
fi

exit "${status}"
