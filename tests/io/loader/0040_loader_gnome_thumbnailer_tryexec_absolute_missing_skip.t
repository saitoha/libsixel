#!/bin/sh
# TAP test covering skip behavior for missing absolute-path TryExec entries.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

if ! feature_defined_in_config "HAVE_FREEDESKTOP_THUMBNAILING"; then
    skip_all "gnome-thumbnailer loader is unavailable on this platform"
fi

echo "1..1"
set -v

input_png="${TOP_SRCDIR}/tests/data/inputs/formats/rgba.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/gnome_tryexec_absolute_missing_skip.sixel"
xdg_data_home="${TOP_BUILDDIR}/tests/data/inputs/thumbnailer/cases/0040"

run_img2sixel --env "XDG_DATA_DIRS=${xdg_data_home}" \
              --env "HOME=${ARTIFACT_LOCAL_DIR}" \
              -v \
              -L gnome-thumbnailer! "${input_png}" >"${output_sixel}" || {
    fail 1 "img2sixel failed"
    exit 0
}

test -s "${output_sixel}" || {
    fail 1 "absolute TryExec skip behavior failed"
    exit 0
}

pass 1 "missing absolute TryExec entry is skipped"
exit 0
