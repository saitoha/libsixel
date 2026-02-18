#!/bin/sh
# TAP test covering zero hint-size environment fallback behavior.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || skip_all "img2sixel is disabled in this build"

test "${HAVE_FREEDESKTOP_THUMBNAILING-}" = 1 || {
    skip_all "gnome-thumbnailer loader is unavailable on this platform"
}

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
    fail 1 "img2sixel failed"
    exit 0
}

default_size=$(cat "${default_log}")

run_img2sixel \
    --env "XDG_DATA_DIRS=${xdg_data_home}" \
    --env "PATH=${bin_dir}:${PATH}" \
    --env "THUMB_LOG=${zero_log}" \
    --env "SIXEL_THUMBNAILER_HINT_SIZE=0" \
    -L gnome-thumbnailer! "${input_png}" >/dev/null || {
    fail 1 "img2sixel failed"
    exit 0
}

zero_size=$(cat "${zero_log}")

test -n "${default_size}" || {
    fail 1 "zero hint-size env fallback check failed"
    exit 0
}

test "${default_size}" = "${zero_size}" || {
    fail 1 "zero hint-size env fallback check failed"
    exit 0
}

pass 1 "zero hint-size env falls back to default"
exit 0
