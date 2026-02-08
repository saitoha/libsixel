#!/bin/sh
# TAP test covering repeated MimeType key handling.

set -eux

script_dir=$(CDPATH=; cd "${0%[/\\]*}" && pwd)
. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

status=0
case_id=1

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

if ! feature_defined_in_config "HAVE_UNISTD_H" || \
        ! feature_defined_in_config "HAVE_SYS_WAIT_H" || \
        ! feature_defined_in_config "HAVE_FORK"; then
    skip_all "gnome-thumbnailer loader is unavailable on this platform"
fi

echo "1..1"
set -v

input_png="${top_srcdir}/tests/data/inputs/formats/rgba.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/gnome_mimetype_last_assignment.sixel"
error_log="${ARTIFACT_LOCAL_DIR}/gnome_mimetype_last_assignment.err"
work_dir="${ARTIFACT_LOCAL_DIR}/gnome_mimetype_last_assignment"
marker_file="${work_dir}/last_mime.marker"
xdg_data_home="${work_dir}/xdg"
bin_dir="${work_dir}/bin"
thumb_dir="${xdg_data_home}/thumbnailers"

rm -rf "${work_dir}"
mkdir -p "${bin_dir}" "${thumb_dir}"

cat >"${bin_dir}/fake-thumb" <<'EOS2'
#!/bin/sh
set -eu
cp "$1" "$2"
: >"${THUMB_MARKER:?}"
EOS2
chmod +x "${bin_dir}/fake-thumb"

cat >"${thumb_dir}/mime-overwrite.thumbnailer" <<'EOS2'
[Thumbnailer Entry]
TryExec=fake-thumb
Exec=fake-thumb %i %o
MimeType=text/plain;
MimeType=image/png;
EOS2

if run_img2sixel \
        --env "XDG_DATA_DIRS=${xdg_data_home}" \
        --env "PATH=${bin_dir}:${PATH}" \
        --env "THUMB_MARKER=${marker_file}" \
        -L gnome-thumbnailer! "${input_png}" \
        >"${output_sixel}" 2>"${error_log}" && \
        [ -s "${output_sixel}" ] && [ -f "${marker_file}" ]; then
    pass "${case_id}" "last MimeType assignment is applied"
else
    fail "${case_id}" "repeated MimeType assignment handling failed"
    status=1
fi

exit "${status}"
