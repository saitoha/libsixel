#!/bin/sh
# TAP test covering quote and escape parsing in Exec templates.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

if ! feature_defined_in_config "HAVE_FREEDESKTOP_THUMBNAILING"; then
    skip_all "gnome-thumbnailer loader is unavailable on this platform"
fi

echo "1..1"
set -v

input_png="${top_srcdir}/tests/data/inputs/formats/rgba.png"
log_file="${ARTIFACT_LOCAL_DIR}/gnome_exec_quoting_args.log"
template_root="${top_srcdir}/tests/data/inputs/thumbnailer"
xdg_data_home="${template_root}/cases/0038"
bin_dir="${template_root}/bin"

run_img2sixel --env "XDG_DATA_DIRS=${xdg_data_home}" \
              --env "PATH=${bin_dir}:${PATH}" \
              --env "THUMB_LOG=${log_file}" \
              -L gnome-thumbnailer! "${input_png}" >/dev/null || {
    fail 1 "img2sixel failed"
    exit 0
}

awk '
    $0 == "a3=value with space" { ++cnt; }
    $0 == "a4=escaped token" { ++cnt; }
    $0 == "a5=%" { ++cnt; }
    END { if (cnt != 3) exit 1; }
' "${log_file}" || {
    fail 1 "Exec quote and escape parsing failed"
    exit 0
}

pass 1 "Exec quote and escape tokens are parsed correctly"
exit 0
