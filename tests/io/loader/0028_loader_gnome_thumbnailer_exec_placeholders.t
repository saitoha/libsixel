#!/bin/sh
# TAP test covering Exec placeholder expansion in gnome-thumbnailer entries.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

if ! feature_defined_in_config "HAVE_FREEDESKTOP_THUMBNAILING"; then
    skip_all "gnome-thumbnailer loader is unavailable on this platform"
fi

echo "1..1"
set -v

input_png="${top_srcdir}/tests/data/inputs/formats/rgba.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/gnome_exec_placeholders.sixel"
error_log="${ARTIFACT_LOCAL_DIR}/gnome_exec_placeholders.err"
log_file="${ARTIFACT_LOCAL_DIR}/gnome_exec_placeholders.log"
template_root="${top_srcdir}/tests/data/inputs/thumbnailer"
xdg_data_home="${template_root}/cases/0028"
bin_dir="${template_root}/bin"

run_img2sixel --env "XDG_DATA_DIRS=${xdg_data_home}" \
              --env "PATH=${bin_dir}:${PATH}" \
              --env "THUMB_LOG=${log_file}" \
              --env "SIXEL_THUMBNAILER_HINT_SIZE=123" \
              -L gnome-thumbnailer! "${input_png}" -o/dev/null || {
    fail 1 "img2sixel failed"
    exit 0
}

awk '
    /^uri=file:\/\// { ++cnt; }
    /^size=123$/ { ++cnt; }
    /^mime=image\/png$/ { ++cnt; }
    /^percent=%$/ { ++cnt; }
    END { if (cnt != 4) exit 1; }
' "${log_file}" || {
    fail 1 "gnome-thumbnailer Exec placeholder test failed"
    exit 0
}

pass 1 "gnome-thumbnailer Exec placeholders are expanded"
exit 0
