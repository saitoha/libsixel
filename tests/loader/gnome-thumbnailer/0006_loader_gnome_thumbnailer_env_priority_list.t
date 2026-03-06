#!/bin/sh
# TAP test covering SIXEL_LOADER_PRIORITY_LIST without -L override.

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
output_sixel="${ARTIFACT_LOCAL_DIR}/gnome_env_priority.sixel"
template_root="${TOP_SRCDIR}/tests/data/inputs/thumbnailer"
xdg_data_home="${template_root}/cases/0030"
bin_dir="${template_root}/bin"

msg=$(set +xv; run_img2sixel \
        --env "XDG_DATA_DIRS=${xdg_data_home}" \
        --env "PATH=${bin_dir}:${PATH}" \
        --env "SIXEL_LOADER_PRIORITY_LIST=gnome-thumbnailer!" \
        "${input_png}" >"${output_sixel}" 2>&1) || {
    echo "not ok" 1 - "env priority list test failed"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg}" >&2
    exit 0
}

test -s "${output_sixel}" || {
    echo "not ok" 1 - "env priority list test failed"
    exit 0
}

echo "ok" 1 - "env priority list enables gnome-thumbnailer"

exit 0
