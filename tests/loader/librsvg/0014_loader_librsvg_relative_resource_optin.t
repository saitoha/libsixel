#!/bin/sh
# TAP test confirming relative external resources are opt-in for librsvg.

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

fixture_dir="${ARTIFACT_LOCAL_DIR}/librsvg-relative-assets"
svg_path="${fixture_dir}/relative-image.svg"
linked_png="${fixture_dir}/linked.png"
default_sixel="${ARTIFACT_LOCAL_DIR}/librsvg-relative-default.six"
optin_sixel="${ARTIFACT_LOCAL_DIR}/librsvg-relative-optin.six"
header_alpha="${ARTIFACT_LOCAL_DIR}/librsvg-relative-header-alpha.bin"
header_opaque="${ARTIFACT_LOCAL_DIR}/librsvg-relative-header-opaque.bin"

mkdir -p "${fixture_dir}"
cp "${TOP_SRCDIR}/tests/data/inputs/formats/rgb.png" "${linked_png}"

cat >"${svg_path}" <<'SVGEOF'
<svg xmlns='http://www.w3.org/2000/svg'
     xmlns:xlink='http://www.w3.org/1999/xlink'
     width='8' height='8' viewBox='0 0 8 8'>
  <image x='0' y='0' width='8' height='8' preserveAspectRatio='none'
         href='linked.png' xlink:href='linked.png' />
</svg>
SVGEOF

printf '\033P0;1q' >"${header_alpha}"
printf '\033Pq' >"${header_opaque}"

run_img2sixel -L librsvg! "${svg_path}" >"${default_sixel}" || {
    echo "not ok" 1 - "default relative-resource SVG conversion failed"
    exit 0
}

run_img2sixel --env SIXEL_LOADER_LIBRSVG_ALLOW_RELATIVE_RESOURCES=1 \
              -L librsvg! "${svg_path}" >"${optin_sixel}" || {
    echo "not ok" 1 - "opt-in relative-resource SVG conversion failed"
    exit 0
}

dd if="${default_sixel}" bs=1 count=6 2>/dev/null | cmp -s - "${header_alpha}" || {
    echo "not ok" 1 - "default path unexpectedly resolved relative resource"
    exit 0
}

dd if="${optin_sixel}" bs=1 count=3 2>/dev/null | cmp -s - "${header_opaque}" || {
    echo "not ok" 1 - "opt-in path did not resolve relative resource"
    exit 0
}

dd if="${optin_sixel}" bs=1 count=6 2>/dev/null | cmp -s - "${header_alpha}" && {
    echo "not ok" 1 - "opt-in path unexpectedly kept transparent header"
    exit 0
}

cmp -s "${default_sixel}" "${optin_sixel}" && {
    echo "not ok" 1 - "relative-resource opt-in did not change output"
    exit 0
}

echo "ok" 1 - "librsvg relative-resource opt-in gating works"
exit 0
