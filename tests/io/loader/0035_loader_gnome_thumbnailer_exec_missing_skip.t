#!/bin/sh
# TAP test covering missing Exec entry skip behavior.

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
output_sixel="${ARTIFACT_LOCAL_DIR}/gnome_exec_missing_skip.sixel"
error_log="${ARTIFACT_LOCAL_DIR}/gnome_exec_missing_skip.err"
work_dir="${ARTIFACT_LOCAL_DIR}/gnome_exec_missing_skip"
marker_file="${work_dir}/valid_entry.marker"
xdg_data_home="${work_dir}/xdg"
bin_dir="${work_dir}/bin"
thumb_dir="${xdg_data_home}/thumbnailers"

rm -rf "${work_dir}"
mkdir -p "${bin_dir}" "${thumb_dir}"

cat >"${bin_dir}/fake-thumb" <<'EOS'
#!/bin/sh
set -eu
cp "$1" "$2"
: >"${VALID_MARKER:?}"
EOS
chmod +x "${bin_dir}/fake-thumb"

cat >"${thumb_dir}/entry-missing-exec.thumbnailer" <<'EOS'
[Thumbnailer Entry]
TryExec=fake-thumb
MimeType=image/png;
EOS

cat >"${thumb_dir}/entry-valid.thumbnailer" <<'EOS'
[Thumbnailer Entry]
TryExec=fake-thumb
Exec=fake-thumb %i %o
MimeType=image/png;
EOS

if run_img2sixel \
        --env "XDG_DATA_DIRS=${xdg_data_home}" \
        --env "PATH=${bin_dir}:${PATH}" \
        --env "VALID_MARKER=${marker_file}" \
        -L gnome-thumbnailer! "${input_png}" \
        >"${output_sixel}" 2>"${error_log}" && \
        [ -s "${output_sixel}" ] && [ -f "${marker_file}" ]; then
    pass "${case_id}" "missing Exec entry is skipped"
else
    fail "${case_id}" "missing Exec skip behavior failed"
    status=1
fi

exit "${status}"
