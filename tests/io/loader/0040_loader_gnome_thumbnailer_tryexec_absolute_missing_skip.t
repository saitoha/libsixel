#!/bin/sh
# TAP test covering skip behavior for missing absolute-path TryExec entries.

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
output_sixel="${ARTIFACT_LOCAL_DIR}/gnome_tryexec_absolute_missing_skip.sixel"
error_log="${ARTIFACT_LOCAL_DIR}/gnome_tryexec_absolute_missing_skip.err"
work_dir="${ARTIFACT_LOCAL_DIR}/gnome_tryexec_absolute_missing_skip"
xdg_data_home="${work_dir}/xdg"
bin_dir="${work_dir}/bin"
thumb_dir="${xdg_data_home}/thumbnailers"
template_root="${top_srcdir}/tests/data/inputs/thumbnailer"
missing_tryexec="${bin_dir}/missing-thumb"
valid_exec="${bin_dir}/fake-thumb"

rm -rf "${work_dir}"
mkdir -p "${bin_dir}" "${thumb_dir}"

cp "${template_root}/bin/fake-thumb" "${valid_exec}"
chmod +x "${valid_exec}"
cp "${template_root}/thumbnailers/missing-absolute.thumbnailer" \
        "${thumb_dir}/missing-absolute.thumbnailer"
cp "${template_root}/thumbnailers/valid-absolute.thumbnailer" \
        "${thumb_dir}/valid-absolute.thumbnailer"
sed -i "s|@MISSING_TRYEXEC@|${missing_tryexec}|g" \
        "${thumb_dir}/missing-absolute.thumbnailer"
sed -i "s|@VALID_EXEC@|${valid_exec}|g" \
        "${thumb_dir}/valid-absolute.thumbnailer"

if run_img2sixel \
        --env "XDG_DATA_DIRS=${xdg_data_home}" \
        --env "PATH=${PATH}" \
        -L gnome-thumbnailer! "${input_png}" \
        >"${output_sixel}" 2>"${error_log}" && \
        [ -s "${output_sixel}" ]; then
    pass "${case_id}" "missing absolute TryExec entry is skipped"
else
    fail "${case_id}" "absolute TryExec skip behavior failed"
    status=1
fi

exit "${status}"
