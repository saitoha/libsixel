#!/bin/sh
# TAP test covering zero hint-size environment fallback behavior.

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
output_sixel="${ARTIFACT_LOCAL_DIR}/gnome_hint_zero_env.sixel"
error_log="${ARTIFACT_LOCAL_DIR}/gnome_hint_zero_env.err"
work_dir="${ARTIFACT_LOCAL_DIR}/gnome_hint_zero_env"
xdg_data_home="${work_dir}/xdg"
bin_dir="${work_dir}/bin"
thumb_dir="${xdg_data_home}/thumbnailers"
default_log="${work_dir}/size_default.log"
zero_log="${work_dir}/size_zero.log"
default_size=""
zero_size=""

rm -rf "${work_dir}"
mkdir -p "${bin_dir}" "${thumb_dir}"

cat >"${bin_dir}/fake-thumb" <<'EOS'
#!/bin/sh
set -eu
cp "$1" "$2"
printf '%s\n' "$3" >"${THUMB_LOG:?}"
EOS
chmod +x "${bin_dir}/fake-thumb"

cat >"${thumb_dir}/hint-size-zero.thumbnailer" <<'EOS'
[Thumbnailer Entry]
TryExec=fake-thumb
Exec=fake-thumb %i %o %s
MimeType=image/png;
EOS

run_img2sixel \
    --env "XDG_DATA_DIRS=${xdg_data_home}" \
    --env "PATH=${bin_dir}:${PATH}" \
    --env "THUMB_LOG=${default_log}" \
    -L gnome-thumbnailer! "${input_png}" >"${output_sixel}" 2>"${error_log}"

default_size=$(cat "${default_log}")

run_img2sixel \
    --env "XDG_DATA_DIRS=${xdg_data_home}" \
    --env "PATH=${bin_dir}:${PATH}" \
    --env "THUMB_LOG=${zero_log}" \
    --env "SIXEL_THUMBNAILER_HINT_SIZE=0" \
    -L gnome-thumbnailer! "${input_png}" >"${output_sixel}" 2>"${error_log}"

zero_size=$(cat "${zero_log}")

if [ -n "${default_size}" ] && [ "${default_size}" = "${zero_size}" ]; then
    pass "${case_id}" "zero hint-size env falls back to default"
else
    fail "${case_id}" "zero hint-size env fallback check failed"
    status=1
fi

exit "${status}"
