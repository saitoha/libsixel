#!/bin/sh
# TAP test covering -L overriding SIXEL_LOADER_PRIORITY_LIST.

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
output_sixel="${ARTIFACT_LOCAL_DIR}/gnome_cli_overrides_env.sixel"
template_root="${TOP_SRCDIR}/tests/data/inputs/thumbnailer"
xdg_data_home="${template_root}/cases/0031"
bin_dir="${template_root}/bin"

run_img2sixel \
        --env "XDG_DATA_DIRS=${xdg_data_home}" \
        --env "PATH=${bin_dir}:${PATH}" \
        --env "SIXEL_LOADER_PRIORITY_LIST=builtin!" \
        -L gnome-thumbnailer! "${input_png}" \
        >"${output_sixel}" || {
    echo "not ok" 1 "CLI override for loader priority list failed"
    exit 0
}

test -s "${output_sixel}" || {
    echo "not ok" 1 "CLI override for loader priority list failed"
    exit 0
}

echo "ok" 1 "-L overrides SIXEL_LOADER_PRIORITY_LIST"

exit 0
