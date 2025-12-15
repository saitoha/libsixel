#!/bin/sh
# TAP test verifying that resize planning always converts to linear RGB
# float32 before scaling when the working colorspace changes. The planner
# should insert colorspace filters on both sides of the scaler and declare
# the linear input format in the verbose DAG dump.

set -euxv

# Common setup mirroring other converter tests.
test_name=$(basename "$0")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${test_name}"
log_file="${artifact_dir}/resize.log"
out_file="${artifact_dir}/resize.six"
ppm_file="${artifact_dir}/resize.ppm"

mkdir -p "${artifact_dir}"

# Keep execution deterministic and avoid palette worker interference.
export SIXEL_THREADS=1

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${script_dir}/converters-common.t"

status=0
case_id=1

pass() {
    printf 'ok %s - %s\n' "$1" "$2"
}

fail() {
    printf 'not ok %s - %s\n' "$1" "$2"
    status=1
}
ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo "1..3"

# Build a small RGB input to drive the resize path.
cat <<'PPM' >"${ppm_file}"
P3
4 4
255
255 0 0   0 255 0   0 0 255   255 255 0
255 0 0   0 255 0   0 0 255   255 255 0
255 0 0   0 255 0   0 0 255   255 255 0
255 0 0   0 255 0   0 0 255   255 255 0
PPM

# Run with a non-sRGB working space so the planner must schedule
# color conversions before and after the scaler.
if run_img2sixel -v -W oklab -w 99% \
        -o "${out_file}" "${ppm_file}" \
        >"${artifact_dir}/stdout.log" 2>"${log_file}"; then
    pass ${case_id} "img2sixel completed with verbose dump"
else
    fail ${case_id} "img2sixel failed with verbose dump"
fi
case_id=$((case_id + 1))

# Verify that the resize section declares linear float32 input.
if grep -q "resize: mode=.*input=linear-f32" "${log_file}"; then
    pass ${case_id} "planner selects linear float32 for scaling"
else
    fail ${case_id} "missing linear float32 resize input"
fi
case_id=$((case_id + 1))

# Ensure colorspace filters bracket the scaler.
if grep -q "colorspace(pre)" "${log_file}" \
        && grep -q "colorspace(post)" "${log_file}"; then
    pass ${case_id} "colorspace conversions placed around scaler"
else
    fail ${case_id} "colorspace filters missing around scaler"
fi

exit ${status}
