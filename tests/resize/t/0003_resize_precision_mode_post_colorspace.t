#!/bin/sh
# TAP test: resize planner places colorspace conversion after scaling.

set -euxv

test_name=$(basename "$0")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${test_name}"
log_file="${artifact_dir}/resize.log"
out_file="${artifact_dir}/resize.six"
ppm_file="${artifact_dir}/resize.ppm"

mkdir -p "${artifact_dir}"

export SIXEL_THREADS=1

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${script_dir}/../../common/t/0001_converters_common.t"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo "1..1"

cat <<'PPM' >"${ppm_file}"
P3
4 4
255
255 0 0   0 255 0   0 0 255   255 255 0
255 0 0   0 255 0   0 0 255   255 255 0
255 0 0   0 255 0   0 0 255   255 255 0
255 0 0   0 255 0   0 0 255   255 255 0
PPM

run_img2sixel -v -W oklab -w 99% \
    -o "${out_file}" "${ppm_file}" \
    >"${artifact_dir}/stdout.log" 2>"${log_file}" || true

if grep -q "scale -> colorspace(post)" "${log_file}" \
        && grep -q "colorspace(post) -> dither" "${log_file}"; then
    printf 'ok 1 - colorspace conversion placed after scaler\n'
else
    printf 'not ok 1 - colorspace conversion missing after scaler\n'
    exit 1
fi
