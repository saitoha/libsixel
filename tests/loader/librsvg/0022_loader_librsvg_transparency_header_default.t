#!/bin/sh
# TAP test confirming default transparent SVG keeps the alpha SIXEL header.

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
mkdir -p "${ARTIFACT_LOCAL_DIR}"

svg_path="${TOP_SRCDIR}/tests/data/inputs/formats/librsvg-transparent-2color.svg"
default_sixel="${ARTIFACT_LOCAL_DIR}/librsvg-transparent-default-header.six"
esc="$(printf '\033')"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L librsvg! "${svg_path}" >"${default_sixel}" || {
    echo "not ok" 1 - "default transparent SVG conversion failed"
    exit 0
}

IFS= read -r sixel_line <"${default_sixel}" || :
test -n "${sixel_line-}" || {
    echo "not ok" 1 - "failed to read transparent SVG SIXEL header"
    exit 0
}
case "${sixel_line}" in
    "${esc}P0;1q"*)
        ;;
    *)
        echo "not ok" 1 - "transparent SVG did not emit ESC P0;1q header"
        exit 0
        ;;
esac

echo "ok" 1 - "default transparent SVG keeps alpha SIXEL header"
exit 0
