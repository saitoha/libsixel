#!/bin/sh
# TAP test covering unknown placeholder handling in Exec templates.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

if ! feature_defined_in_config "HAVE_FREEDESKTOP_THUMBNAILING"; then
    skip_all "gnome-thumbnailer loader is unavailable on this platform"
fi

echo "1..1"
set -v

input_png="${TOP_SRCDIR}/tests/data/inputs/formats/rgba.png"
template_root="${TOP_SRCDIR}/tests/data/inputs/thumbnailer"
log_file="${ARTIFACT_LOCAL_DIR}/gnome_unknown_placeholder.log"
xdg_data_home="${template_root}/cases/0034"
bin_dir="${xdg_data_home}/bin:${template_root}/bin"

run_img2sixel --env "XDG_DATA_DIRS=${xdg_data_home}" \
              --env "PATH=${bin_dir}:${PATH}" \
              --env "THUMB_LOG=${log_file}" \
              -L gnome-thumbnailer! "${input_png}" -o/dev/null || {
    fail 1 "img2sixel failed"
    exit 0
}

grep '^%x$' "${log_file}" >/dev/null 2>&1 || {
    fail 1 "unknown placeholder handling failed"
    exit 0
}

pass 1 "unknown placeholder remains literal"
exit 0
