#!/bin/sh
# TAP test covering TryExec skip logic and fallback thumbnailer execution.

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
output_sixel="${ARTIFACT_LOCAL_DIR}/gnome_tryexec_skip.sixel"
error_log="${ARTIFACT_LOCAL_DIR}/gnome_tryexec_skip.err"
work_dir="${ARTIFACT_LOCAL_DIR}/gnome_tryexec_skip"
xdg_data_home="${work_dir}/xdg"
bin_dir="${work_dir}/bin"
thumb_dir="${xdg_data_home}/thumbnailers"
template_root="${top_srcdir}/tests/data/inputs/thumbnailer"

rm -rf "${work_dir}"
mkdir -p "${bin_dir}" "${thumb_dir}"

cp "${template_root}/bin/fake-thumb" "${bin_dir}/fake-thumb"
cp "${template_root}/bin/gdk-pixbuf-thumbnailer" \
        "${bin_dir}/gdk-pixbuf-thumbnailer"
chmod +x "${bin_dir}/fake-thumb" "${bin_dir}/gdk-pixbuf-thumbnailer"
cp "${template_root}/thumbnailers/tryexec-missing.thumbnailer" \
        "${thumb_dir}/tryexec-missing.thumbnailer"
cp "${template_root}/thumbnailers/fallback-gdk.thumbnailer" \
        "${thumb_dir}/fallback.thumbnailer"

if run_img2sixel \
        --env "XDG_DATA_DIRS=${xdg_data_home}" \
        --env "PATH=${bin_dir}:${PATH}" \
        -L gnome-thumbnailer! "${input_png}" \
        >"${output_sixel}" 2>"${error_log}" && \
        [ -s "${output_sixel}" ]; then
    pass "${case_id}" "TryExec mismatch skips entry and uses fallback"
else
    fail "${case_id}" "TryExec skip/fallback path failed"
    status=1
fi

exit "${status}"
