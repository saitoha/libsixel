#!/bin/sh
# TAP test covering wildcard MIME matching in gnome-thumbnailer entries.

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
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_png="${TOP_SRCDIR}/tests/data/inputs/formats/rgba.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/gnome_mime_wildcard.sixel"
template_root="${TOP_SRCDIR}/tests/data/inputs/thumbnailer"
xdg_data_home="${template_root}/cases/0027"
bin_dir="${template_root}/bin"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env "XDG_DATA_DIRS=${xdg_data_home}" \
              --env "PATH=${bin_dir}:${PATH}" \
              -L gnome-thumbnailer! "${input_png}" >"${output_sixel}" || {
    echo "not ok" 1 - "img2sixel failed"
    exit 0
}

test -s "${output_sixel}" || {
    echo "not ok" 1 - "empty output"
    exit 0
}

echo "ok" 1 - "gnome-thumbnailer wildcard MIME entry matches PNG"
exit 0
