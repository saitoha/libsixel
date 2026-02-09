#!/bin/sh
# TAP test covering unknown placeholder handling in Exec templates.

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
output_sixel="${ARTIFACT_LOCAL_DIR}/gnome_unknown_placeholder.sixel"
error_log="${ARTIFACT_LOCAL_DIR}/gnome_unknown_placeholder.err"
log_file="${ARTIFACT_LOCAL_DIR}/gnome_unknown_placeholder.log"
template_root="${top_srcdir}/tests/data/inputs/thumbnailer"
xdg_data_home="${template_root}/cases/0034"
bin_dir="${xdg_data_home}/bin:${template_root}/bin"

if run_img2sixel \
        --env "XDG_DATA_DIRS=${xdg_data_home}" \
        --env "PATH=${bin_dir}:${PATH}" \
        --env "THUMB_LOG=${log_file}" \
        -L gnome-thumbnailer! "${input_png}" \
        >"${output_sixel}" 2>"${error_log}" && \
        [ -s "${output_sixel}" ] && \
        grep '^%x$' "${log_file}" >/dev/null 2>&1; then
    pass "${case_id}" "unknown placeholder remains literal"
else
    fail "${case_id}" "unknown placeholder handling failed"
    status=1
fi

exit "${status}"
