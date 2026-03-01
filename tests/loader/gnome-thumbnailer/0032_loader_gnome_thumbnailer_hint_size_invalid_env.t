#!/bin/sh
# TAP test covering invalid SIXEL_THUMBNAILER_HINT_SIZE fallback behavior.

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
xdg_data_home="${template_root}/cases/0032"
bin_dir="${xdg_data_home}/bin:${template_root}/bin"
default_log="${ARTIFACT_LOCAL_DIR}/gnome_hint_invalid_default.log"
invalid_log="${ARTIFACT_LOCAL_DIR}/gnome_hint_invalid_invalid.log"

run_img2sixel \
    --env "XDG_DATA_DIRS=${xdg_data_home}" \
    --env "PATH=${bin_dir}:${PATH}" \
    --env "THUMB_LOG=${default_log}" \
    -L gnome-thumbnailer! "${input_png}" >/dev/null

default_size=$(cat "${default_log}")

run_img2sixel \
    --env "XDG_DATA_DIRS=${xdg_data_home}" \
    --env "PATH=${bin_dir}:${PATH}" \
    --env "THUMB_LOG=${invalid_log}" \
    --env "SIXEL_THUMBNAILER_HINT_SIZE=abc" \
    -L gnome-thumbnailer! "${input_png}" >/dev/null

invalid_size=$(cat "${invalid_log}")

test -n "${default_size}" || {
    echo "not ok" 1 "invalid size env fallback check failed"
    exit 0
}

test "${default_size}" = "${invalid_size}" || {
    echo "not ok" 1 "invalid size env fallback check failed"
    exit 0
}

echo "ok" 1 "invalid size env falls back to default hint"
exit 0
