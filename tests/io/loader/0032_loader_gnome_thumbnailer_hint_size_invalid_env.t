#!/bin/sh
# TAP test covering invalid SIXEL_THUMBNAILER_HINT_SIZE fallback behavior.

set -eux

script_dir=$(CDPATH=; cd "${0%[/\\]*}" && pwd)
. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

status=0
case_id=1

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

if ! feature_defined_in_config "HAVE_FREEDESKTOP_THUMBNAILING"; then
    skip_all "gnome-thumbnailer loader is unavailable on this platform"
fi

echo "1..1"
set -v

input_png="${top_srcdir}/tests/data/inputs/formats/rgba.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/gnome_hint_invalid_env.sixel"
error_log="${ARTIFACT_LOCAL_DIR}/gnome_hint_invalid_env.err"
template_root="${top_srcdir}/tests/data/inputs/thumbnailer"
xdg_data_home="${template_root}/cases/0032"
bin_dir="${xdg_data_home}/bin:${template_root}/bin"
default_log="${ARTIFACT_LOCAL_DIR}/gnome_hint_invalid_default.log"
invalid_log="${ARTIFACT_LOCAL_DIR}/gnome_hint_invalid_invalid.log"
default_size=""
invalid_size=""

run_img2sixel \
    --env "XDG_DATA_DIRS=${xdg_data_home}" \
    --env "PATH=${bin_dir}:${PATH}" \
    --env "THUMB_LOG=${default_log}" \
    -L gnome-thumbnailer! "${input_png}" >"${output_sixel}" 2>"${error_log}"

default_size=$(cat "${default_log}")

run_img2sixel \
    --env "XDG_DATA_DIRS=${xdg_data_home}" \
    --env "PATH=${bin_dir}:${PATH}" \
    --env "THUMB_LOG=${invalid_log}" \
    --env "SIXEL_THUMBNAILER_HINT_SIZE=abc" \
    -L gnome-thumbnailer! "${input_png}" >"${output_sixel}" 2>"${error_log}"

invalid_size=$(cat "${invalid_log}")

if [ -n "${default_size}" ] && [ "${default_size}" = "${invalid_size}" ]; then
    pass "${case_id}" "invalid size env falls back to default hint"
else
    fail "${case_id}" "invalid size env fallback check failed"
    status=1
fi

exit "${status}"
