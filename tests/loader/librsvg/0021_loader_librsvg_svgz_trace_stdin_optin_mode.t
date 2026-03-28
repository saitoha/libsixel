#!/bin/sh
# TAP test confirming stdin .svgz opt-in path emits trace decode_mode=stdin_svgz_tempfile.

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
trace_stdin_optin_err="${ARTIFACT_LOCAL_DIR}/librsvg-svgz-trace-stdin-optin.err"

gzip -c "${svg_path}" >"${svgz_path}"

SIXEL_TRACE_TOPIC=loader \
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_LOADER_LIBRSVG_ALLOW_STDIN_SVGZ=1 \
    -L librsvg! - \
    >/dev/null 2>"${trace_stdin_optin_err}" \
    <"${svgz_path}" || {
    echo "not ok" 1 - "trace-enabled stdin .svgz opt-in conversion failed"
    exit 0
}

grep -F "librsvg: decode_mode=stdin_svgz_tempfile" \
    "${trace_stdin_optin_err}" >/dev/null || {
    echo "not ok" 1 - "stdin .svgz opt-in trace mode was not reported"
    exit 0
}

echo "ok" 1 - "librsvg stdin .svgz opt-in trace decode mode is reported"
exit 0
