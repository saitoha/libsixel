#!/bin/sh
# TAP test covering INI group scoping for thumbnailer keys.

set -eux

script_dir=$(CDPATH=; cd "${0%[/\\]*}" && pwd)
. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

status=0
case_id=1

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

if ! feature_defined_in_config "HAVE_FREEDESKTOP_THUMBNAILING"; then
    skip_all "gnome-thumbnailer loader is unavailable on this platform"
fi

echo "1..1"
set -v

input_png="${top_srcdir}/tests/data/inputs/formats/rgba.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/gnome_group_scope.sixel"
error_log="${ARTIFACT_LOCAL_DIR}/gnome_group_scope.err"
template_root="${top_srcdir}/tests/data/inputs/thumbnailer"
xdg_data_home="${template_root}/cases/0043"
bin_dir="${template_root}/bin"




if run_img2sixel \
        --env "XDG_DATA_DIRS=${xdg_data_home}" \
        --env "PATH=${bin_dir}:${PATH}" \
        -L gnome-thumbnailer! "${input_png}" \
        >"${output_sixel}" 2>"${error_log}" && \
        [ -s "${output_sixel}" ]; then
    pass "${case_id}" "keys outside Thumbnailer Entry are ignored"
else
    fail "${case_id}" "group-scoped parser behavior failed"
    status=1
fi

exit "${status}"
