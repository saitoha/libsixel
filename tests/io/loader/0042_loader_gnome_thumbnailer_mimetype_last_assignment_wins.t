#!/bin/sh
# TAP test covering repeated MimeType key handling.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

if ! feature_defined_in_config "HAVE_FREEDESKTOP_THUMBNAILING"; then
    skip_all "gnome-thumbnailer loader is unavailable on this platform"
fi

echo "1..1"
set -v

input_png="${top_srcdir}/tests/data/inputs/formats/rgba.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/gnome_mimetype_last_assignment.sixel"
template_root="${top_srcdir}/tests/data/inputs/thumbnailer"
xdg_data_home="${template_root}/cases/0042"
bin_dir="${template_root}/bin"

run_img2sixel --env "XDG_DATA_DIRS=${xdg_data_home}" \
              --env "PATH=${bin_dir}:${PATH}" \
              -L gnome-thumbnailer! "${input_png}" >/dev/null || {
    fail 1 "repeated MimeType assignment handling failed"
    exit 0
}

pass 1 "last MimeType assignment is applied"
exit 0
