#!/bin/sh
# TAP test covering zero hint-size environment fallback behavior.

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
xdg_data_home="${template_root}/cases/0037"
bin_dir="${xdg_data_home}/bin:${template_root}/bin"
default_log="${ARTIFACT_LOCAL_DIR}/gnome_hint_zero_default.log"
zero_log="${ARTIFACT_LOCAL_DIR}/gnome_hint_zero_zero.log"

run_img2sixel \
    --env "XDG_DATA_DIRS=${xdg_data_home}" \
    --env "PATH=${bin_dir}:${PATH}" \
    --env "THUMB_LOG=${default_log}" \
    -L gnome-thumbnailer! "${input_png}" >/dev/null || {
    echo "not ok" 1 - "img2sixel failed"
    exit 0
}

default_size=$(cat "${default_log}")

run_img2sixel \
    --env "XDG_DATA_DIRS=${xdg_data_home}" \
    --env "PATH=${bin_dir}:${PATH}" \
    --env "THUMB_LOG=${zero_log}" \
    --env "SIXEL_THUMBNAILER_HINT_SIZE=0" \
    -L gnome-thumbnailer! "${input_png}" >/dev/null || {
    echo "not ok" 1 - "img2sixel failed"
    exit 0
}

zero_size=$(cat "${zero_log}")

test -n "${default_size}" || {
    echo "not ok" 1 - "zero hint-size env fallback check failed"
    exit 0
}

test "${default_size}" = "${zero_size}" || {
    echo "not ok" 1 - "zero hint-size env fallback check failed"
    exit 0
}

echo "ok" 1 - "zero hint-size env falls back to default"
exit 0
