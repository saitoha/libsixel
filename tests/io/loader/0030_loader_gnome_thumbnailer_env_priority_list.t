#!/bin/sh
# TAP test covering SIXEL_LOADER_PRIORITY_LIST without -L override.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

feature_defined_in_config "HAVE_FREEDESKTOP_THUMBNAILING" || {
    skip_all "gnome-thumbnailer loader is unavailable on this platform"
}

echo "1..1"
set -v

input_png="${TOP_SRCDIR}/tests/data/inputs/formats/rgba.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/gnome_env_priority.sixel"
error_log="${ARTIFACT_LOCAL_DIR}/gnome_env_priority.err"
template_root="${TOP_SRCDIR}/tests/data/inputs/thumbnailer"
xdg_data_home="${template_root}/cases/0030"
bin_dir="${template_root}/bin"

run_img2sixel \
        --env "XDG_DATA_DIRS=${xdg_data_home}" \
        --env "PATH=${bin_dir}:${PATH}" \
        --env "SIXEL_LOADER_PRIORITY_LIST=gnome-thumbnailer!" \
        "${input_png}" >"${output_sixel}" 2>"${error_log}" || {
    fail 1 "env priority list test failed"
    exit 0
}

test -s "${output_sixel}" || {
    fail 1 "env priority list test failed"
    exit 0
}

pass 1 "env priority list enables gnome-thumbnailer"

exit 0
