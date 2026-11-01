#!/bin/sh
# TAP test confirming stdin .svgz is rejected by default for librsvg.

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

svgz_path="${TOP_SRCDIR}/tests/data/inputs/formats/librsvg-transparent-2color.svgz"
status=0
msg=$(
    set +xv
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L librsvg! - -o/dev/null 2>&1 \
        <"${svgz_path}"
) || status="$?"

test "${status}" -ne 0 || {
    echo "not ok" 1 - "stdin .svgz conversion unexpectedly succeeded"
    exit 0
}

found=0
while IFS= read -r line; do
    case "${line}" in
        *"gzip-compressed SVG (.svgz) requires file-path decode or prior decompression."* | \
        *"gzip-compressed SVG (.svgz) requires file-path decode, prior decompression, or SIXEL_LOADER_LIBRSVG_ALLOW_STDIN_SVGZ=1."*)
            found=1
            ;;
    esac
done <<__MSG__
${msg}
__MSG__

test "${found}" -eq 1 || {
    echo "not ok" 1 - "stdin .svgz failure did not report decode policy"
    exit 0
}

echo "ok" 1 - "librsvg stdin .svgz decode is rejected by default"
exit 0
