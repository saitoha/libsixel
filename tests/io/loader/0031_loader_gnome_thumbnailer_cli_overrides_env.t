#!/bin/sh
# TAP test covering -L overriding SIXEL_LOADER_PRIORITY_LIST.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

feature_defined_in_config "HAVE_FREEDESKTOP_THUMBNAILING" || {
    skip_all "gnome-thumbnailer loader is unavailable on this platform"
}

echo "1..1"
set -v

input_png="${top_srcdir}/tests/data/inputs/formats/rgba.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/gnome_cli_overrides_env.sixel"
template_root="${top_srcdir}/tests/data/inputs/thumbnailer"
xdg_data_home="${template_root}/cases/0031"
bin_dir="${template_root}/bin"

run_img2sixel \
        --env "XDG_DATA_DIRS=${xdg_data_home}" \
        --env "PATH=${bin_dir}:${PATH}" \
        --env "SIXEL_LOADER_PRIORITY_LIST=builtin!" \
        -L gnome-thumbnailer! "${input_png}" \
        >"${output_sixel}" || {
    fail 1 "CLI override for loader priority list failed"
    exit 0
}

test -s "${output_sixel}" || {
    fail 1 "CLI override for loader priority list failed"
    exit 0
}

pass 1 "-L overrides SIXEL_LOADER_PRIORITY_LIST"

exit 0
