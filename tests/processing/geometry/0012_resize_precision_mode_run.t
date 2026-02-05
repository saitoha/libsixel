#!/bin/sh
# TAP test: img2sixel runs with resize planner verbose dump enabled.

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

if run_img2sixel -v -W oklab -w 99% \
        -o "${out_file}" "${ppm_file}" \
        >"${ARTIFACT_LOCAL_DIR}/stdout.log"; then
    printf 'ok 1 - img2sixel completed with verbose dump\n'
else
    printf 'not ok 1 - img2sixel failed with verbose dump\n'
    exit 1
fi
