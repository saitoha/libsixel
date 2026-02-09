#!/bin/sh
# TAP test covering quote and escape parsing in Exec templates.

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
output_sixel="${ARTIFACT_LOCAL_DIR}/gnome_exec_quoting_escape.sixel"
error_log="${ARTIFACT_LOCAL_DIR}/gnome_exec_quoting_escape.err"
work_dir="${ARTIFACT_LOCAL_DIR}/gnome_exec_quoting_escape"
log_file="${work_dir}/args.log"
xdg_data_home="${work_dir}/xdg"
bin_dir="${work_dir}/bin"
thumb_dir="${xdg_data_home}/thumbnailers"
template_root="${top_srcdir}/tests/data/inputs/thumbnailer"

rm -rf "${work_dir}"
mkdir -p "${bin_dir}" "${thumb_dir}"

cp "${template_root}/bin/fake-thumb-args" "${bin_dir}/fake-thumb-args"
chmod +x "${bin_dir}/fake-thumb-args"

cp "${template_root}/thumbnailers/exec-quoting.thumbnailer" "${thumb_dir}/exec-quoting.thumbnailer"

if run_img2sixel \
        --env "XDG_DATA_DIRS=${xdg_data_home}" \
        --env "PATH=${bin_dir}:${PATH}" \
        --env "THUMB_LOG=${log_file}" \
        -L gnome-thumbnailer! "${input_png}" \
        >"${output_sixel}" 2>"${error_log}" && \
        [ -s "${output_sixel}" ] && \
        grep '^a3=value with space$' "${log_file}" >/dev/null 2>&1 && \
        grep '^a4=escaped token$' "${log_file}" >/dev/null 2>&1 && \
        grep '^a5=%$' "${log_file}" >/dev/null 2>&1; then
    pass "${case_id}" "Exec quote and escape tokens are parsed correctly"
else
    fail "${case_id}" "Exec quote and escape parsing failed"
    status=1
fi

exit "${status}"
