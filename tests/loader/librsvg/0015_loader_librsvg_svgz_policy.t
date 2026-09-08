#!/bin/sh
# TAP test confirming local .svgz file-path decode succeeds for librsvg.

set -eux

test "${HAVE_LIBRSVG-}" = 1 || {
    printf "1..0 # SKIP librsvg loader is unavailable in this build\n"
    exit 0
}

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

svgz_path="${TOP_SRCDIR}/tests/data/inputs/formats/librsvg-transparent-2color.svgz"
file_sixel="${ARTIFACT_LOCAL_DIR}/librsvg-svgz-file.six"
esc="$(printf '\033')"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L librsvg! "${svgz_path}" >"${file_sixel}" || {
    echo "not ok" 1 - "file-path .svgz conversion failed"
    exit 0
}

IFS= read -r sixel_line <"${file_sixel}" || :
test -n "${sixel_line-}" || {
    echo "not ok" 1 - "failed to read file-path .svgz SIXEL header"
    exit 0
}
case "${sixel_line}" in
    "${esc}P0;0q"*)
        ;;
    *)
        echo "not ok" 1 - "file-path .svgz conversion lost transparency header"
        exit 0
        ;;
esac

echo "ok" 1 - "librsvg .svgz file-path decode works"
exit 0
