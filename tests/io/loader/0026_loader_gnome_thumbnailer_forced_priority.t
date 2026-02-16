#!/bin/sh
# TAP test covering forced gnome-thumbnailer execution with fallback disabled.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

feature_defined_in_config "HAVE_FREEDESKTOP_THUMBNAILING" || {
    skip_all "gnome-thumbnailer loader is unavailable on this platform"
}

echo "1..1"
set -v

input_png="${top_srcdir}/tests/data/inputs/formats/rgba.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/gnome_forced_priority.sixel"
error_log="${ARTIFACT_LOCAL_DIR}/gnome_forced_priority.err"
template_root="${top_srcdir}/tests/data/inputs/thumbnailer"
xdg_data_home="${template_root}/cases/0026"
bin_dir="${template_root}/bin"

set +e
run_img2sixel \
        --env "XDG_DATA_DIRS=${xdg_data_home}" \
        --env "PATH=${bin_dir}:${PATH}" \
        -L gnome-thumbnailer! "${input_png}" >"${output_sixel}" 2>"${error_log}"
status=$?
set -e

test "${status}" -eq 0 || grep "gnome-thumbnailer\|thumbnailer" \
        "${error_log}" >/dev/null 2>&1 || {
    fail 1 "forced gnome-thumbnailer loader path failed"
    exit 0
}

test "${status}" -eq 0 || {
    fail 1 "gnome-thumbnailer runtime should be available"
    exit 0
}

test -s "${output_sixel}" || {
    fail 1 "forced gnome-thumbnailer loader path failed"
    exit 0
}

pass 1 "forced gnome-thumbnailer loader path succeeds"

exit 0
