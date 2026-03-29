#!/bin/sh
# TAP test covering quote and escape parsing in Exec templates.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}
test "${HAVE_FREEDESKTOP_THUMBNAILING-}" = 1 || {
    printf "1..0 # SKIP gnome-thumbnailer loader is unavailable on this platform\n"
    exit 0
}


echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_png="${TOP_SRCDIR}/tests/data/inputs/formats/rgba.png"
log_file="${ARTIFACT_LOCAL_DIR}/gnome_exec_quoting_args.log"
template_root="${TOP_SRCDIR}/tests/data/inputs/thumbnailer"
xdg_data_home="${template_root}/cases/0038"
bin_dir="${template_root}/bin"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env "XDG_DATA_DIRS=${xdg_data_home}" \
              --env "PATH=${bin_dir}:${PATH}" \
              --env "THUMB_LOG=${log_file}" \
              -L gnome-thumbnailer! "${input_png}" >/dev/null || {
    echo "not ok" 1 - "img2sixel failed"
    exit 0
}

has_a3=0
has_a4=0
has_a5=0
while IFS= read -r line; do
    case "${line}" in
        *"a3=value with space"*)
            has_a3=1
            ;;
        *"a4=escaped token"*)
            has_a4=1
            ;;
        *"a5=%"*)
            has_a5=1
            ;;
    esac
done < "${log_file}"

test "${has_a3}" -eq 1 || {
    echo "not ok" 1 - "Exec quote and escape parsing failed"
    exit 0
}
test "${has_a4}" -eq 1 || {
    echo "not ok" 1 - "Exec quote and escape parsing failed"
    exit 0
}
test "${has_a5}" -eq 1 || {
    echo "not ok" 1 - "Exec quote and escape parsing failed"
    exit 0
}

echo "ok" 1 - "Exec quote and escape tokens are parsed correctly"
exit 0
