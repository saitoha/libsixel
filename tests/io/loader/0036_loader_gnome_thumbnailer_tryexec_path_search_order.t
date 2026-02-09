#!/bin/sh
# TAP test covering TryExec PATH search order behavior.

set -eux

script_dir=$(CDPATH=; cd "${0%[/\\]*}" && pwd)
. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

status=0
case_id=1

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

if ! feature_defined_in_config "HAVE_UNISTD_H" || \
        ! feature_defined_in_config "HAVE_SYS_WAIT_H" || \
        ! feature_defined_in_config "HAVE_FORK" || \
        feature_defined_in_config "HAVE_EMSCRIPTEN_H"; then
    skip_all "gnome-thumbnailer loader is unavailable on this platform"
fi

echo "1..1"
set -v

input_png="${top_srcdir}/tests/data/inputs/formats/rgba.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/gnome_tryexec_path_order.sixel"
error_log="${ARTIFACT_LOCAL_DIR}/gnome_tryexec_path_order.err"
work_dir="${ARTIFACT_LOCAL_DIR}/gnome_tryexec_path_order"
marker_file="${work_dir}/path_hit.marker"
xdg_data_home="${work_dir}/xdg"
bin_dir_1="${work_dir}/bin1"
bin_dir_2="${work_dir}/bin2"
thumb_dir="${xdg_data_home}/thumbnailers"

rm -rf "${work_dir}"
mkdir -p "${bin_dir_1}" "${bin_dir_2}" "${thumb_dir}"

cat >"${bin_dir_2}/fake-thumb" <<'EOS'
#!/bin/sh
set -eu
cp "$1" "$2"
: >"${PATH_MARKER:?}"
EOS
chmod +x "${bin_dir_2}/fake-thumb"

cat >"${thumb_dir}/path-order.thumbnailer" <<'EOS'
[Thumbnailer Entry]
TryExec=fake-thumb
Exec=fake-thumb %i %o
MimeType=image/png;
EOS

if run_img2sixel \
        --env "XDG_DATA_DIRS=${xdg_data_home}" \
        --env "PATH=${bin_dir_1}:${bin_dir_2}:${PATH}" \
        --env "PATH_MARKER=${marker_file}" \
        -L gnome-thumbnailer! "${input_png}" \
        >"${output_sixel}" 2>"${error_log}" && \
        [ -s "${output_sixel}" ] && [ -f "${marker_file}" ]; then
    pass "${case_id}" "TryExec PATH search finds command in later entry"
else
    fail "${case_id}" "TryExec PATH search order test failed"
    status=1
fi

exit "${status}"
