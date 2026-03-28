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

command -v gzip >/dev/null 2>&1 || {
    printf "1..0 # SKIP gzip is unavailable in this environment\n"
    exit 0
}

echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

svg_path="${TOP_SRCDIR}/tests/data/inputs/formats/librsvg-transparent-2color.svg"
svgz_path="${ARTIFACT_LOCAL_DIR}/librsvg-transparent-2color.svgz"
stdin_sixel="${ARTIFACT_LOCAL_DIR}/librsvg-svgz-stdin.six"
stdin_err="${ARTIFACT_LOCAL_DIR}/librsvg-svgz-stdin.err"

gzip -c "${svg_path}" >"${svgz_path}"

set +e
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L librsvg! - >"${stdin_sixel}" 2>"${stdin_err}" <"${svgz_path}"
status="$?"
set -e

test "${status}" -ne 0 || {
    echo "not ok" 1 - "stdin .svgz conversion unexpectedly succeeded"
    exit 0
}

grep -F "gzip-compressed SVG (.svgz) requires file-path decode or prior decompression." \
    "${stdin_err}" >/dev/null \
    || grep -F "gzip-compressed SVG (.svgz) requires file-path decode, prior decompression, or SIXEL_LOADER_LIBRSVG_ALLOW_STDIN_SVGZ=1." \
        "${stdin_err}" >/dev/null || {
    echo "not ok" 1 - "stdin .svgz failure did not report decode policy"
    exit 0
}

echo "ok" 1 - "librsvg stdin .svgz decode is rejected by default"
exit 0
