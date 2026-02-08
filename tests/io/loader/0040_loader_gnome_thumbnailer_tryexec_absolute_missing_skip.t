#!/bin/sh
# TAP test covering skip behavior for missing absolute-path TryExec entries.

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
output_sixel="${ARTIFACT_LOCAL_DIR}/gnome_tryexec_absolute_missing_skip.sixel"
error_log="${ARTIFACT_LOCAL_DIR}/gnome_tryexec_absolute_missing_skip.err"
work_dir="${ARTIFACT_LOCAL_DIR}/gnome_tryexec_absolute_missing_skip"
marker_file="${work_dir}/fallback.marker"
xdg_data_home="${work_dir}/xdg"
bin_dir="${work_dir}/bin"
thumb_dir="${xdg_data_home}/thumbnailers"
missing_tryexec="${bin_dir}/missing-thumb"
valid_exec="${bin_dir}/fake-thumb"

rm -rf "${work_dir}"
mkdir -p "${bin_dir}" "${thumb_dir}"

cat >"${valid_exec}" <<'EOS2'
#!/bin/sh
set -eu
cp "$1" "$2"
: >"${VALID_MARKER:?}"
EOS2
chmod +x "${valid_exec}"

cat >"${thumb_dir}/missing-absolute.thumbnailer" <<EOS2
[Thumbnailer Entry]
TryExec=${missing_tryexec}
Exec=${missing_tryexec} %i %o
MimeType=image/png;
EOS2

cat >"${thumb_dir}/valid-absolute.thumbnailer" <<EOS2
[Thumbnailer Entry]
TryExec=${valid_exec}
Exec=${valid_exec} %i %o
MimeType=image/png;
EOS2

if run_img2sixel \
        --env "XDG_DATA_DIRS=${xdg_data_home}" \
        --env "PATH=/usr/bin:/bin" \
        --env "VALID_MARKER=${marker_file}" \
        -L gnome-thumbnailer! "${input_png}" \
        >"${output_sixel}" 2>"${error_log}" && \
        [ -s "${output_sixel}" ] && [ -f "${marker_file}" ]; then
    pass "${case_id}" "missing absolute TryExec entry is skipped"
else
    fail "${case_id}" "absolute TryExec skip behavior failed"
    status=1
fi

exit "${status}"
