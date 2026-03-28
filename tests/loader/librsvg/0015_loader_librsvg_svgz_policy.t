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

echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

svg_path="${TOP_SRCDIR}/tests/data/inputs/formats/librsvg-transparent-2color.svg"
svgz_path="${ARTIFACT_LOCAL_DIR}/librsvg-transparent-2color.svgz"
file_sixel="${ARTIFACT_LOCAL_DIR}/librsvg-svgz-file.six"
stdin_sixel="${ARTIFACT_LOCAL_DIR}/librsvg-svgz-stdin.six"
stdin_err="${ARTIFACT_LOCAL_DIR}/librsvg-svgz-stdin.err"
stdin_optin_sixel="${ARTIFACT_LOCAL_DIR}/librsvg-svgz-stdin-optin.six"
trace_file_err="${ARTIFACT_LOCAL_DIR}/librsvg-svgz-trace-file.err"
trace_stdin_reject_err="${ARTIFACT_LOCAL_DIR}/librsvg-svgz-trace-stdin-reject.err"
trace_stdin_optin_err="${ARTIFACT_LOCAL_DIR}/librsvg-svgz-trace-stdin-optin.err"
header_alpha="${ARTIFACT_LOCAL_DIR}/librsvg-svgz-header-alpha.bin"

gzip -c "${svg_path}" >"${svgz_path}"
printf '\033P0;1q' >"${header_alpha}"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L librsvg! "${svgz_path}" >"${file_sixel}" || {
    echo "not ok" 1 - "file-path .svgz conversion failed"
    exit 0
}

dd if="${file_sixel}" bs=1 count=6 2>/dev/null | cmp -s - "${header_alpha}" || {
    echo "not ok" 1 - "file-path .svgz conversion lost transparency header"
    exit 0
}

set +e
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L librsvg! - >"${stdin_sixel}" 2>"${stdin_err}" <"${svgz_path}"
status="$?"
set -e

test "${status}" -ne 0 || {
    echo "not ok" 1 - "stdin .svgz conversion unexpectedly succeeded"
    exit 0
}

grep -F "gzip-compressed SVG (.svgz) requires file-path decode or prior decompression." \
    "${stdin_err}" >/dev/null || {
    grep -F "gzip-compressed SVG (.svgz) requires file-path decode, prior decompression, or SIXEL_LOADER_LIBRSVG_ALLOW_STDIN_SVGZ=1." \
        "${stdin_err}" >/dev/null || {
    echo "not ok" 1 - "stdin .svgz failure did not report decode policy"
    exit 0
}
}

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

SIXEL_TRACE_TOPIC=loader ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L librsvg! "${svgz_path}" \
    >/dev/null 2>"${trace_file_err}" || {
    echo "not ok" 1 - "trace-enabled file-path .svgz conversion failed"
    exit 0
}

grep -F "librsvg: decode_mode=file" "${trace_file_err}" >/dev/null || {
    echo "not ok" 1 - "file-path .svgz trace mode was not reported"
    exit 0
}

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

echo "ok" 1 - "librsvg .svgz file/stdin opt-in decode policy works"
exit 0
