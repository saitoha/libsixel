#!/bin/sh
# TAP test covering TryExec skip logic and fallback thumbnailer execution.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || skip_all "img2sixel is disabled in this build"

test "${HAVE_FREEDESKTOP_THUMBNAILING-}" = 1 || {
    skip_all "gnome-thumbnailer loader is unavailable on this platform"
}

echo "1..1"
set -v

input_png="${TOP_SRCDIR}/tests/data/inputs/formats/rgba.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/gnome_tryexec_skip.sixel"
template_root="${TOP_SRCDIR}/tests/data/inputs/thumbnailer"
xdg_data_home="${template_root}/cases/0029"
bin_dir="${template_root}/bin"

run_img2sixel \
        --env "XDG_DATA_DIRS=${xdg_data_home}" \
        --env "PATH=${bin_dir}:${PATH}" \
        -L gnome-thumbnailer! "${input_png}" \
        >"${output_sixel}" || {
    fail 1 "TryExec skip/fallback path failed"
    exit 0
}

test -s "${output_sixel}" || {
    fail 1 "TryExec skip/fallback path failed"
    exit 0
}

pass 1 "TryExec mismatch skips entry and uses fallback"

exit 0
