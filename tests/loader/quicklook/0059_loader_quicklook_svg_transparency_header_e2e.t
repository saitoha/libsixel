#!/bin/sh
# TAP test confirming QuickLook keeps/composites SVG transparency headers.

set -eux

test "${HAVE_QUICKLOOK-}" = 1 || {
    printf "1..0 # SKIP quicklook loader is unavailable\n"
    exit 0
}

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

svg_path="${TOP_SRCDIR}/tests/data/inputs/formats/librsvg-transparent-2color.svg"
default_sixel="${ARTIFACT_LOCAL_DIR}/quicklook-transparent-default.six"
bg_sixel="${ARTIFACT_LOCAL_DIR}/quicklook-transparent-bg.six"
header_alpha="${ARTIFACT_LOCAL_DIR}/quicklook-header-alpha.bin"
header_opaque="${ARTIFACT_LOCAL_DIR}/quicklook-header-opaque.bin"

printf '\033P0;1q' >"${header_alpha}"
printf '\033Pq' >"${header_opaque}"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L quicklook! "${svg_path}" >"${default_sixel}" || {
    echo "not ok" 1 - "default transparent SVG conversion failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L quicklook! -B '#ffffff' "${svg_path}" >"${bg_sixel}" || {
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

echo "ok" 1 - "quicklook transparency header routing is correct end-to-end"
exit 0
