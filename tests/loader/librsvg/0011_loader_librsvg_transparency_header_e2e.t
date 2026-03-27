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

test "${HAVE_SIXEL2PNG-}" = 1 || {
    printf "1..0 # SKIP sixel2png is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

svg_path="${TOP_SRCDIR}/tests/data/inputs/formats/librsvg-transparent-2color.svg"
default_sixel="${ARTIFACT_LOCAL_DIR}/librsvg-transparent-default.six"
bg_sixel="${ARTIFACT_LOCAL_DIR}/librsvg-transparent-bg.six"
default_png="${ARTIFACT_LOCAL_DIR}/librsvg-transparent-default.png"
bg_png="${ARTIFACT_LOCAL_DIR}/librsvg-transparent-bg.png"
header_alpha="${ARTIFACT_LOCAL_DIR}/librsvg-header-alpha.bin"
header_opaque="${ARTIFACT_LOCAL_DIR}/librsvg-header-opaque.bin"
png_plte_magic="${ARTIFACT_LOCAL_DIR}/librsvg-png-plte-magic.bin"
png_plte_len_two="${ARTIFACT_LOCAL_DIR}/librsvg-png-plte-len-two.bin"
png_plte_prefix_red_white="${ARTIFACT_LOCAL_DIR}/librsvg-png-plte-prefix-red-white.bin"

printf '\033P0;1q' >"${header_alpha}"
printf '\033Pq' >"${header_opaque}"
printf 'PLTE' >"${png_plte_magic}"
printf '\000\000\000\006' >"${png_plte_len_two}"
printf '\377\003\003\377\377\377' >"${png_plte_prefix_red_white}"

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

run_sixel2png -i "${default_sixel}" -o "${default_png}" || {
    echo "not ok" 1 - "default transparent SIXEL decode failed"
    exit 0
}

run_sixel2png -i "${bg_sixel}" -o "${bg_png}" || {
    echo "not ok" 1 - "background-composited SIXEL decode failed"
    exit 0
}

cmp -s "${default_png}" "${bg_png}" && {
    echo "not ok" 1 - "default/background PNG decode unexpectedly matched"
    exit 0
}

dd if="${default_png}" bs=1 skip=37 count=4 2>/dev/null | cmp -s - "${png_plte_magic}" || {
    echo "not ok" 1 - "default PNG did not contain expected PLTE chunk"
    exit 0
}

dd if="${bg_png}" bs=1 skip=37 count=4 2>/dev/null | cmp -s - "${png_plte_magic}" || {
    echo "not ok" 1 - "background PNG did not contain expected PLTE chunk"
    exit 0
}

dd if="${bg_png}" bs=1 skip=33 count=4 2>/dev/null | cmp -s - "${png_plte_len_two}" || {
    echo "not ok" 1 - "background PNG palette length was not 2 colors"
    exit 0
}

dd if="${default_png}" bs=1 skip=33 count=4 2>/dev/null | cmp -s - "${png_plte_len_two}" && {
    echo "not ok" 1 - "default PNG unexpectedly collapsed to 2-color palette"
    exit 0
}

expected_plte_cksum_line="$(cksum "${png_plte_prefix_red_white}")"
IFS=' ' read -r expected_plte_crc expected_plte_len _ <<EOF
${expected_plte_cksum_line}
EOF
expected_plte_tag="${expected_plte_crc}:${expected_plte_len}"

bg_plte_cksum_line="$(dd if="${bg_png}" bs=1 skip=41 count=6 2>/dev/null | cksum)"
IFS=' ' read -r bg_plte_crc bg_plte_len _ <<EOF
${bg_plte_cksum_line}
EOF
bg_plte_tag="${bg_plte_crc}:${bg_plte_len}"
test "${bg_plte_tag}" = "${expected_plte_tag}" || {
    echo "not ok" 1 - "background PNG palette prefix did not include red+white"
    exit 0
}

default_plte_cksum_line="$(dd if="${default_png}" bs=1 skip=41 count=6 2>/dev/null | cksum)"
IFS=' ' read -r default_plte_crc default_plte_len _ <<EOF
${default_plte_cksum_line}
EOF
default_plte_tag="${default_plte_crc}:${default_plte_len}"
test "${default_plte_tag}" != "${expected_plte_tag}" || {
    echo "not ok" 1 - "default PNG unexpectedly matched red+white palette prefix"
    exit 0
}

echo "ok" 1 - "librsvg transparency header routing is correct end-to-end"
exit 0
