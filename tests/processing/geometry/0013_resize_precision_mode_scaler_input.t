#!/bin/sh
# TAP test: resize planner reports the scaler input pixel format.

set -eux

out_file="${ARTIFACT_LOCAL_DIR}/resize.six"
ppm_file="${ARTIFACT_LOCAL_DIR}/resize.ppm"


export SIXEL_THREADS=1

script_dir=${test_dir}
. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

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

resize_log=$(run_img2sixel -v -W oklab -w 99% \
    -o "${out_file}" "${ppm_file}" 2>&1 || true)
printf '%s' "${resize_log}" >&2

if printf '%s' "${resize_log}" \
        | grep -q "resize: mode=.*input=linear-f32"; then
    printf 'ok 1 - planner reports scaler input pixelformat\n'
else
    printf 'not ok 1 - missing scaler input declaration\n'
    exit 1
fi
