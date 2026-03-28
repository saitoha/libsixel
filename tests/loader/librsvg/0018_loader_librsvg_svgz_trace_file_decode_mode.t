#!/bin/sh
# TAP test confirming file-path .svgz decode emits trace decode_mode=file.

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
trace_file_err="${ARTIFACT_LOCAL_DIR}/librsvg-svgz-trace-file.err"

gzip -c "${svg_path}" >"${svgz_path}"

SIXEL_TRACE_TOPIC=loader ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L librsvg! "${svgz_path}" \
    >/dev/null 2>"${trace_file_err}" || {
    echo "not ok" 1 - "trace-enabled file-path .svgz conversion failed"
    exit 0
}

grep -F "librsvg: decode_mode=file" "${trace_file_err}" >/dev/null || {
    echo "not ok" 1 - "file-path .svgz trace mode was not reported"
    exit 0
}

echo "ok" 1 - "librsvg .svgz file-path trace decode mode is reported"
exit 0
