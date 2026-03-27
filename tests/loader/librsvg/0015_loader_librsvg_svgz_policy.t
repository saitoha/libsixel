#!/bin/sh
# TAP test confirming librsvg .svgz decode policy.

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

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

svg_path="${TOP_SRCDIR}/tests/data/inputs/formats/librsvg-transparent-2color.svg"
svgz_path="${ARTIFACT_LOCAL_DIR}/librsvg-transparent-2color.svgz"
file_sixel="${ARTIFACT_LOCAL_DIR}/librsvg-svgz-file.six"
stdin_sixel="${ARTIFACT_LOCAL_DIR}/librsvg-svgz-stdin.six"
stdin_err="${ARTIFACT_LOCAL_DIR}/librsvg-svgz-stdin.err"
header_alpha="${ARTIFACT_LOCAL_DIR}/librsvg-svgz-header-alpha.bin"

gzip -c "${svg_path}" >"${svgz_path}"
printf '\033P0;1q' >"${header_alpha}"

run_img2sixel -L librsvg! "${svgz_path}" >"${file_sixel}" || {
    echo "not ok" 1 - "file-path .svgz conversion failed"
    exit 0
}

dd if="${file_sixel}" bs=1 count=6 2>/dev/null | cmp -s - "${header_alpha}" || {
    echo "not ok" 1 - "file-path .svgz conversion lost transparency header"
    exit 0
}

set +e
run_img2sixel -L librsvg! - >"${stdin_sixel}" 2>"${stdin_err}" <"${svgz_path}"
status="$?"
set -e

test "${status}" -ne 0 || {
    echo "not ok" 1 - "stdin .svgz conversion unexpectedly succeeded"
    exit 0
}

grep -F "gzip-compressed SVG (.svgz) requires file-path decode or prior decompression." \
    "${stdin_err}" >/dev/null || {
    echo "not ok" 1 - "stdin .svgz failure did not report decode policy"
    exit 0
}

echo "ok" 1 - "librsvg .svgz file-path decode policy works"
exit 0
