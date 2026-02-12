#!/bin/sh
# TAP test: resize planner places colorspace conversion after scaling.

set -eux

export SIXEL_THREADS=1

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

artifact_dir="${ARTIFACT_LOCAL_DIR:-/tmp}"
out_file="${artifact_dir}/resize.six"
ppm_file="${artifact_dir}/resize.ppm"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo "1..1"
set -v

cat <<'PPM' >"${ppm_file}"
P3
4 4
255
255 0 0   0 255 0   0 0 255   255 255 0
255 0 0   0 255 0   0 0 255   255 255 0
255 0 0   0 255 0   0 0 255   255 255 0
255 0 0   0 255 0   0 0 255   255 255 0
PPM

resize_log=$(run_img2sixel -v -W oklab -w 99%     -o "${out_file}" "${ppm_file}" 2>&1) || {
    fail 1 "img2sixel failed while collecting resize planner log"
    exit 0
}
printf '%s' "${resize_log}" >&2

printf '%s' "${resize_log}" | grep -q "scale -> colorspace(post)" || {
    fail 1 "missing scale -> colorspace(post)"
    exit 0
}

printf '%s' "${resize_log}" | grep -q "colorspace(post) -> dither" || {
    fail 1 "missing colorspace(post) -> dither"
    exit 0
}

pass 1 "colorspace conversion placed after scaler"

exit 0
