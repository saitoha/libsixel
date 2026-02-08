#!/bin/sh
# TAP test covering empty TryExec behavior.

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
output_sixel="${ARTIFACT_LOCAL_DIR}/gnome_tryexec_empty_allowed.sixel"
error_log="${ARTIFACT_LOCAL_DIR}/gnome_tryexec_empty_allowed.err"
work_dir="${ARTIFACT_LOCAL_DIR}/gnome_tryexec_empty_allowed"
marker_file="${work_dir}/empty_tryexec.marker"
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

cat >"${thumb_dir}/empty-tryexec.thumbnailer" <<'EOS2'
[Thumbnailer Entry]
TryExec=
Exec=fake-thumb %i %o
MimeType=image/png;
EOS2

if run_img2sixel \
        --env "XDG_DATA_DIRS=${xdg_data_home}" \
        --env "PATH=${bin_dir}:${PATH}" \
        --env "THUMB_MARKER=${marker_file}" \
        -L gnome-thumbnailer! "${input_png}" \
        >"${output_sixel}" 2>"${error_log}" && \
        [ -s "${output_sixel}" ] && [ -f "${marker_file}" ]; then
    pass "${case_id}" "empty TryExec does not block execution"
else
    fail "${case_id}" "empty TryExec behavior failed"
    status=1
fi

exit "${status}"
