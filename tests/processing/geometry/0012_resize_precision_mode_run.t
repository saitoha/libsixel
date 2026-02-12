#!/bin/sh
# TAP test: img2sixel runs with resize planner verbose dump enabled.

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

run_img2sixel -v -W oklab -w 99%         -o "${out_file}" "${ppm_file}"         >"${artifact_dir}/stdout.log" || {
    fail 1 "img2sixel failed with verbose dump"
    exit 0
}

pass 1 "img2sixel completed with verbose dump"

exit 0
