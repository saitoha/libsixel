#!/bin/sh
# TAP test covering absolute-path TryExec acceptance.

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
output_sixel="${ARTIFACT_LOCAL_DIR}/gnome_tryexec_absolute_path.sixel"
error_log="${ARTIFACT_LOCAL_DIR}/gnome_tryexec_absolute_path.err"
work_dir="${ARTIFACT_LOCAL_DIR}/gnome_tryexec_absolute_path"
xdg_data_home="${work_dir}/xdg"
bin_dir="${work_dir}/bin"
thumb_dir="${xdg_data_home}/thumbnailers"
template_root="${top_srcdir}/tests/data/inputs/thumbnailer"
tryexec_path="${bin_dir}/fake-thumb"
template_file="${thumb_dir}/absolute-tryexec.thumbnailer"

rm -rf "${work_dir}"
mkdir -p "${bin_dir}" "${thumb_dir}"

cp "${template_root}/bin/fake-thumb" "${tryexec_path}"
chmod +x "${tryexec_path}"
cp "${template_root}/thumbnailers/absolute-tryexec.thumbnailer" \
        "${template_file}"

# Avoid sed -i because BSD sed and GNU sed parse -i arguments differently.
# Replace placeholders with Python so paths containing '/' work everywhere.
python3 - "${template_file}" "${tryexec_path}" <<'PY'
import pathlib
import sys

path = pathlib.Path(sys.argv[1])
tryexec = sys.argv[2]
text = path.read_text(encoding="utf-8")
path.write_text(text.replace("@TRYEXEC_PATH@", tryexec), encoding="utf-8")
PY

if run_img2sixel \
        --env "XDG_DATA_DIRS=${xdg_data_home}" \
        --env "PATH=${PATH}" \
        -L gnome-thumbnailer! "${input_png}" \
        >"${output_sixel}" 2>"${error_log}" && \
        [ -s "${output_sixel}" ]; then
    pass "${case_id}" "absolute TryExec path is accepted"
else
    fail "${case_id}" "absolute TryExec path handling failed"
    status=1
fi

exit "${status}"
