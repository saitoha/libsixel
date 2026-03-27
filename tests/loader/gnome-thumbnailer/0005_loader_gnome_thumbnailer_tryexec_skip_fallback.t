#!/bin/sh
# TAP test covering TryExec skip logic and fallback thumbnailer execution.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}
test "${HAVE_FREEDESKTOP_THUMBNAILING-}" = 1 || {
    printf "1..0 # SKIP gnome-thumbnailer loader is unavailable on this platform\n"
    exit 0
}


echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_png="${TOP_SRCDIR}/tests/data/inputs/formats/rgba.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/gnome_tryexec_skip.sixel"
template_root="${TOP_SRCDIR}/tests/data/inputs/thumbnailer"
xdg_data_home="${template_root}/cases/0029"
bin_dir="${template_root}/bin"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --env "XDG_DATA_DIRS=${xdg_data_home}" \
        --env "PATH=${bin_dir}:${PATH}" \
        -L gnome-thumbnailer! "${input_png}" \
        >"${output_sixel}" || {
    echo "not ok" 1 - "TryExec skip/fallback path failed"
    exit 0
}

test -s "${output_sixel}" || {
    echo "not ok" 1 - "TryExec skip/fallback path failed"
    exit 0
}

echo "ok" 1 - "TryExec mismatch skips entry and uses fallback"

exit 0
