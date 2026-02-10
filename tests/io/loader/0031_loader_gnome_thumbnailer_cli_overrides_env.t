#!/bin/sh
# TAP test covering -L overriding SIXEL_LOADER_PRIORITY_LIST.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

if ! feature_defined_in_config "HAVE_FREEDESKTOP_THUMBNAILING"; then
    skip_all "gnome-thumbnailer loader is unavailable on this platform"
fi

echo "1..1"
set -v

input_png="${top_srcdir}/tests/data/inputs/formats/rgba.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/gnome_cli_overrides_env.sixel"
template_root="${top_srcdir}/tests/data/inputs/thumbnailer"
xdg_data_home="${template_root}/cases/0031"
bin_dir="${template_root}/bin"

if run_img2sixel \
        --env "XDG_DATA_DIRS=${xdg_data_home}" \
        --env "PATH=${bin_dir}:${PATH}" \
        --env "SIXEL_LOADER_PRIORITY_LIST=builtin!" \
        -L gnome-thumbnailer! "${input_png}" \
        >"${output_sixel}" && \
        [ -s "${output_sixel}" ]; then
    pass 1 "-L overrides SIXEL_LOADER_PRIORITY_LIST"
else
    fail 1 "CLI override for loader priority list failed"
fi

exit 0
