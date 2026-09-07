#!/bin/sh
# TAP test confirming default librsvg path blocks relative external resources.

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

svg_path="${TOP_SRCDIR}/tests/data/inputs/formats/librsvg-relative-image.svg"
default_sixel="${ARTIFACT_LOCAL_DIR}/librsvg-relative-default.six"
esc="$(printf '\033')"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L librsvg! "${svg_path}" >"${default_sixel}" || {
    echo "not ok" 1 - "default relative-resource SVG conversion failed"
    exit 0
}

IFS= read -r sixel_line <"${default_sixel}" || :
test -n "${sixel_line-}" || {
    echo "not ok" 1 - "failed to read default relative-resource SIXEL header"
    exit 0
}
case "${sixel_line}" in
    "${esc}P0;1q"*)
        ;;
    *)
        echo "not ok" 1 - "default path unexpectedly resolved relative resource"
        exit 0
        ;;
esac

echo "ok" 1 - "default librsvg path blocks relative resource decode"
exit 0
