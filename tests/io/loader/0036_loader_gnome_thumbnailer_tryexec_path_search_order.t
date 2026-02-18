#!/bin/sh
# TAP test covering TryExec PATH search order behavior.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build";
    exit 0
}

test "${HAVE_FREEDESKTOP_THUMBNAILING-}" = 1 || {
    skip_all "gnome-thumbnailer loader is unavailable on this platform"
}

echo "1..1"
set -v

input_png="${TOP_SRCDIR}/tests/data/inputs/formats/rgba.png"
template_root="${TOP_SRCDIR}/tests/data/inputs/thumbnailer"
xdg_data_home="${template_root}/cases/0036"
bin_dir_1="${template_root}/bin-empty"
bin_dir_2="${template_root}/bin"

run_img2sixel --env "XDG_DATA_DIRS=${xdg_data_home}" \
              --env "PATH=${bin_dir_1}:${bin_dir_2}:${PATH}" \
              -L gnome-thumbnailer! "${input_png}" >/dev/null || {
    fail 1 "TryExec PATH search order test failed"
    exit 0
}

pass 1 "TryExec PATH search finds command in later entry"
exit 0
