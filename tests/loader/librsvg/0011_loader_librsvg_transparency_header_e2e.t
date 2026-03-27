#!/bin/sh
# TAP test confirming final img2sixel output keeps/transforms transparency
# headers for librsvg input as expected.

set -eux

test "${HAVE_LIBRSVG-}" = 1 || {
    printf "1..0 # SKIP librsvg loader is unavailable in this build\n"
    exit 0
}

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

svg_path="${TOP_SRCDIR}/tests/data/inputs/formats/librsvg-transparent-2color.svg"
default_sixel="${ARTIFACT_LOCAL_DIR}/librsvg-transparent-default.six"
bg_sixel="${ARTIFACT_LOCAL_DIR}/librsvg-transparent-bg.six"
header_alpha="${ARTIFACT_LOCAL_DIR}/librsvg-header-alpha.bin"
header_opaque="${ARTIFACT_LOCAL_DIR}/librsvg-header-opaque.bin"

printf '\033P0;1q' >"${header_alpha}"
printf '\033Pq' >"${header_opaque}"

run_img2sixel -L librsvg! "${svg_path}" >"${default_sixel}" || {
    echo "not ok" 1 - "default transparent SVG conversion failed"
    exit 0
}

run_img2sixel -L librsvg! -B '#ffffff' "${svg_path}" >"${bg_sixel}" || {
    echo "not ok" 1 - "background-composited SVG conversion failed"
    exit 0
}

dd if="${default_sixel}" bs=1 count=6 2>/dev/null | cmp -s - "${header_alpha}" || {
    echo "not ok" 1 - "transparent SVG did not emit ESC P0;1q header"
    exit 0
}

dd if="${bg_sixel}" bs=1 count=3 2>/dev/null | cmp -s - "${header_opaque}" || {
    echo "not ok" 1 - "background SVG did not emit ESC Pq header"
    exit 0
}

dd if="${bg_sixel}" bs=1 count=6 2>/dev/null | cmp -s - "${header_alpha}" && {
    echo "not ok" 1 - "background SVG unexpectedly kept ESC P0;1q header"
    exit 0
}

echo "ok" 1 - "librsvg transparency header routing is correct end-to-end"
exit 0
