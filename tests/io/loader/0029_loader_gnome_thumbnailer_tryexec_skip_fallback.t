#!/bin/sh
# TAP test covering TryExec skip logic and fallback thumbnailer execution.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

feature_defined_in_config "HAVE_FREEDESKTOP_THUMBNAILING" || {
    skip_all "gnome-thumbnailer loader is unavailable on this platform"
}

echo "1..1"
set -v

input_png="${top_srcdir}/tests/data/inputs/formats/rgba.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/gnome_tryexec_skip.sixel"
template_root="${top_srcdir}/tests/data/inputs/thumbnailer"
xdg_data_home="${template_root}/cases/0029"
bin_dir="${template_root}/bin"

run_img2sixel \
        --env "XDG_DATA_DIRS=${xdg_data_home}" \
        --env "PATH=${bin_dir}:${PATH}" \
        -L gnome-thumbnailer! "${input_png}" \
        >"${output_sixel}" || {
    fail 1 "TryExec skip/fallback path failed"
    exit 0
}

test -s "${output_sixel}" || {
    fail 1 "TryExec skip/fallback path failed"
    exit 0
}

pass 1 "TryExec mismatch skips entry and uses fallback"

exit 0
