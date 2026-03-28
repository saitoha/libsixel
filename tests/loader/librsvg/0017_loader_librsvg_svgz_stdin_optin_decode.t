#!/bin/sh
# TAP test confirming stdin .svgz decode succeeds when opt-in is set for librsvg.

set -eux

test "${HAVE_LIBRSVG-}" = 1 || {
    printf "1..0 # SKIP librsvg loader is unavailable in this build\n"
    exit 0
}

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

command -v gzip >/dev/null 2>&1 || {
    printf "1..0 # SKIP gzip is unavailable in this environment\n"
    exit 0
}

echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

svg_path="${TOP_SRCDIR}/tests/data/inputs/formats/librsvg-transparent-2color.svg"
svgz_path="${ARTIFACT_LOCAL_DIR}/librsvg-transparent-2color.svgz"
stdin_optin_sixel="${ARTIFACT_LOCAL_DIR}/librsvg-svgz-stdin-optin.six"
header_alpha="${ARTIFACT_LOCAL_DIR}/librsvg-svgz-header-alpha.bin"

gzip -c "${svg_path}" >"${svgz_path}"
printf '\033P0;1q' >"${header_alpha}"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_LOADER_LIBRSVG_ALLOW_STDIN_SVGZ=1 \
    -L librsvg! - \
    >"${stdin_optin_sixel}" \
    <"${svgz_path}" || {
    echo "not ok" 1 - "stdin .svgz conversion failed with opt-in env"
    exit 0
}

dd if="${stdin_optin_sixel}" bs=1 count=6 2>/dev/null | cmp -s - "${header_alpha}" || {
    echo "not ok" 1 - "stdin .svgz opt-in conversion lost transparency header"
    exit 0
}

echo "ok" 1 - "librsvg stdin .svgz decode works with opt-in"
exit 0
