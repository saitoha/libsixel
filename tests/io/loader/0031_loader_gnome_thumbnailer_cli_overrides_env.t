#!/bin/sh
# TAP test covering -L overriding SIXEL_LOADER_PRIORITY_LIST.

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
output_sixel="${ARTIFACT_LOCAL_DIR}/gnome_cli_overrides_env.sixel"
error_log="${ARTIFACT_LOCAL_DIR}/gnome_cli_overrides_env.err"
work_dir="${ARTIFACT_LOCAL_DIR}/gnome_cli_overrides_env"
xdg_data_home="${work_dir}/xdg"
bin_dir="${work_dir}/bin"
thumb_dir="${xdg_data_home}/thumbnailers"
template_root="${top_srcdir}/tests/data/inputs/thumbnailer"

rm -rf "${work_dir}"
mkdir -p "${bin_dir}" "${thumb_dir}"

cp "${template_root}/bin/fake-thumb" "${bin_dir}/fake-thumb"
chmod +x "${bin_dir}/fake-thumb"

cp "${template_root}/thumbnailers/override-env.thumbnailer" "${thumb_dir}/override-env.thumbnailer"

if run_img2sixel \
        --env "XDG_DATA_DIRS=${xdg_data_home}" \
        --env "PATH=${bin_dir}:${PATH}" \
        --env "SIXEL_LOADER_PRIORITY_LIST=builtin!" \
        -L gnome-thumbnailer! "${input_png}" \
        >"${output_sixel}" 2>"${error_log}" && \
        [ -s "${output_sixel}" ]; then
    pass "${case_id}" "-L overrides SIXEL_LOADER_PRIORITY_LIST"
else
    fail "${case_id}" "CLI override for loader priority list failed"
    status=1
fi

exit "${status}"
