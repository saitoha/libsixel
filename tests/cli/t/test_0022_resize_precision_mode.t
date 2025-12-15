#!/bin/sh
# TAP test verifying that resize planning keeps the scaler input explicit
# in the DAG dump and positions the colorspace conversion after scaling
# when the working colorspace changes. The planner should declare the input
# format and show the post-scale colorspace edge.

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

# Run with a non-sRGB working space so the planner must schedule a
# colorspace conversion around the scaler.
if run_img2sixel -v -W oklab -w 99% \
        -o "${out_file}" "${ppm_file}" \
        >"${artifact_dir}/stdout.log" 2>"${log_file}"; then
    pass ${case_id} "img2sixel completed with verbose dump"
else
    fail ${case_id} "img2sixel failed with verbose dump"
fi
case_id=$((case_id + 1))

# Verify that the resize section declares the scaler input format.
if grep -q "resize: mode=.*input=rgb888" "${log_file}"; then
    pass ${case_id} "planner reports scaler input pixelformat"
else
    fail ${case_id} "missing scaler input declaration"
fi
case_id=$((case_id + 1))

# Ensure the colorspace conversion is scheduled after scaling.
if grep -q "scale -> colorspace(post)" "${log_file}" \
        && grep -q "colorspace(post) -> dither" "${log_file}"; then
    pass ${case_id} "colorspace conversion placed after scaler"
else
    fail ${case_id} "colorspace conversion missing after scaler"
fi

exit ${status}
