#!/bin/sh
# TAP test confirming stdin .svgz reject path emits trace decode_mode=stdin_svgz_rejected.

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
trace_stdin_reject_err="${ARTIFACT_LOCAL_DIR}/librsvg-svgz-trace-stdin-reject.err"

gzip -c "${svg_path}" >"${svgz_path}"

set +e
SIXEL_TRACE_TOPIC=loader ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L librsvg! - \
    >/dev/null 2>"${trace_stdin_reject_err}" <"${svgz_path}"
status="$?"
set -e

test "${status}" -ne 0 || {
    echo "not ok" 1 - "trace-enabled stdin .svgz reject path unexpectedly succeeded"
    exit 0
}

grep -F "librsvg: decode_mode=stdin_svgz_rejected" \
    "${trace_stdin_reject_err}" >/dev/null || {
    echo "not ok" 1 - "stdin .svgz reject trace mode was not reported"
    exit 0
}

echo "ok" 1 - "librsvg stdin .svgz reject trace decode mode is reported"
exit 0
