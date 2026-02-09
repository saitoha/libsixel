#!/bin/sh
# TAP test covering repeated MimeType key handling.

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
output_sixel="${ARTIFACT_LOCAL_DIR}/gnome_mimetype_last_assignment.sixel"
error_log="${ARTIFACT_LOCAL_DIR}/gnome_mimetype_last_assignment.err"
template_root="${top_srcdir}/tests/data/inputs/thumbnailer"
xdg_data_home="${template_root}/cases/0042"
bin_dir="${template_root}/bin"




if run_img2sixel \
        --env "XDG_DATA_DIRS=${xdg_data_home}" \
        --env "PATH=${bin_dir}:${PATH}" \
        -L gnome-thumbnailer! "${input_png}" \
        >"${output_sixel}" 2>"${error_log}" && \
        [ -s "${output_sixel}" ]; then
    pass "${case_id}" "last MimeType assignment is applied"
else
    fail "${case_id}" "repeated MimeType assignment handling failed"
    status=1
fi

exit "${status}"
