#!/bin/sh
# TAP test covering Exec placeholder expansion in gnome-thumbnailer entries.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}
test "${HAVE_FREEDESKTOP_THUMBNAILING-}" = 1 || {
    printf "1..0 # SKIP gnome-thumbnailer loader is unavailable on this platform\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v
mkdir "${ARTIFACT_LOCAL_DIR}"

input_png="${TOP_SRCDIR}/tests/data/inputs/formats/rgba.png"
log_file="${ARTIFACT_LOCAL_DIR}/gnome_exec_placeholders.log"
template_root="${TOP_SRCDIR}/tests/data/inputs/thumbnailer"
xdg_data_home="${template_root}/cases/0028"
bin_dir="${template_root}/bin"

run_img2sixel --env "XDG_DATA_DIRS=${xdg_data_home}" \
              --env "PATH=${bin_dir}:${PATH}" \
              --env "THUMB_LOG=${log_file}" \
              --env "SIXEL_THUMBNAILER_HINT_SIZE=123" \
              -L gnome-thumbnailer! "${input_png}" -o/dev/null || {
    echo "not ok" 1 - "img2sixel failed"
    exit 0
}

awk '/^uri=file:\/\/|^size=123$|^mime=image\/png$|^percent=%$/ { ++cnt; } END { if (cnt != 4) exit 1; } ' "${log_file}" || {
    echo "not ok" 1 - "gnome-thumbnailer Exec placeholder test failed"
    exit 0
}

echo "ok" 1 - "gnome-thumbnailer Exec placeholders are expanded"
exit 0
