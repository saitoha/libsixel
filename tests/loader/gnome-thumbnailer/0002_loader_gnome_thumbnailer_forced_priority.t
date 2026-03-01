#!/bin/sh
# TAP test covering forced gnome-thumbnailer execution with fallback disabled.

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
output_sixel="${ARTIFACT_LOCAL_DIR}/gnome_forced_priority.sixel"
error_log="${ARTIFACT_LOCAL_DIR}/gnome_forced_priority.err"
template_root="${TOP_SRCDIR}/tests/data/inputs/thumbnailer"
xdg_data_home="${template_root}/cases/0026"
bin_dir="${template_root}/bin"

run_img2sixel --env "XDG_DATA_DIRS=${xdg_data_home}" \
              --env "PATH=${bin_dir}:${PATH}" \
              -L gnome-thumbnailer! "${input_png}" >"${output_sixel}" 2>"${error_log}" || status=$?

test "${status-0}" -eq 0 || grep "gnome-thumbnailer\|thumbnailer" \
        "${error_log}" >/dev/null 2>&1 || {
    echo "not ok" 1 "forced gnome-thumbnailer loader path failed"
    exit 0
}
test "${status-0}" -eq 0 || {
    echo "not ok" 1 "gnome-thumbnailer runtime should be available"
    exit 0
}
test -s "${output_sixel}" || {
    echo "not ok" 1 "forced gnome-thumbnailer loader path failed"
    exit 0
}

echo "ok" 1 "forced gnome-thumbnailer loader path succeeds"
exit 0
