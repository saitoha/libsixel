#!/bin/sh
# TAP test covering unknown placeholder handling in Exec templates.

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

input_png="${TOP_SRCDIR}/tests/data/inputs/formats/rgba.png"
template_root="${TOP_SRCDIR}/tests/data/inputs/thumbnailer"
log_file="${ARTIFACT_LOCAL_DIR}/gnome_unknown_placeholder.log"
xdg_data_home="${template_root}/cases/0034"
bin_dir="${xdg_data_home}/bin:${template_root}/bin"

run_img2sixel --env "XDG_DATA_DIRS=${xdg_data_home}" \
              --env "PATH=${bin_dir}:${PATH}" \
              --env "THUMB_LOG=${log_file}" \
              -L gnome-thumbnailer! "${input_png}" -o/dev/null || {
    echo "not ok" 1 "img2sixel failed"
    exit 0
}

awk -v expected="%x" '$0 == expected { found = 1; exit } END { if (!found) exit 1 }' "${log_file}" || {
    echo "not ok" 1 "unknown placeholder handling failed"
    exit 0
}

echo "ok" 1 "unknown placeholder remains literal"
exit 0
