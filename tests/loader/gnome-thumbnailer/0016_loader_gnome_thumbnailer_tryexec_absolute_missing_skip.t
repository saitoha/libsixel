#!/bin/sh
# TAP test covering skip behavior for missing absolute-path TryExec entries.

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
output_sixel="${ARTIFACT_LOCAL_DIR}/gnome_tryexec_absolute_missing_skip.sixel"
xdg_data_home="${TOP_BUILDDIR}/tests/data/inputs/thumbnailer/cases/0040"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env "XDG_DATA_DIRS=${xdg_data_home}" \
              --env "HOME=${ARTIFACT_LOCAL_DIR}" \
              -v \
              -L gnome-thumbnailer! "${input_png}" >"${output_sixel}" || {
    echo "not ok" 1 - "img2sixel failed"
    exit 0
}

test -s "${output_sixel}" || {
    echo "not ok" 1 - "absolute TryExec skip behavior failed"
    exit 0
}

echo "ok" 1 - "missing absolute TryExec entry is skipped"
exit 0
