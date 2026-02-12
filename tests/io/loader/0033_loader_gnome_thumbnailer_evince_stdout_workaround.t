#!/bin/sh
# TAP test covering the evince-thumbnailer stdout workaround path.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

feature_defined_in_config "HAVE_FREEDESKTOP_THUMBNAILING" || {
    skip_all "gnome-thumbnailer loader is unavailable on this platform"
}

echo "1..1"
set -v

input_png="${top_srcdir}/tests/data/inputs/formats/rgba.png"
template_root="${top_srcdir}/tests/data/inputs/thumbnailer"
xdg_data_home="${template_root}/cases/0033"
bin_dir="${template_root}/bin"

run_img2sixel --env "XDG_DATA_DIRS=${xdg_data_home}" \
              --env "PATH=${bin_dir}:${PATH}" \
              -L gnome-thumbnailer! "${input_png}" >/dev/null || {
    fail 1 "evince stdout workaround path failed"
    exit 0
}

pass 1 "evince stdout workaround path succeeds"
exit 0
