#!/bin/sh
# TAP test verifying img2sixel rejects invalid positional inputs without
# emitting stray output.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

# Skip temporarily on Windows environments while addressing
# intermittent failures specific to that platform.
os_name=$(uname -s || echo "unknown")
case "${os_name}" in
    *[Mm][Ii][Nn][Gg][Ww]*|*[Mm][Ss][Yy][Ss]*|*[Cc][Yy][Gg][Ww][Ii][Nn]*)
        printf "1..0 # SKIP temporarily disabled on Windows due to instability\n"
        exit 0
        ;;
esac


echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

missing_path="${ARTIFACT_LOCAL_DIR}/invalid_filename"
missing_output="${ARTIFACT_LOCAL_DIR}/capture.invalid"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_TRACE_TOPIC=file_open:lifecycle \
    -v "${missing_path}" >"${missing_output}" && {
    echo "not ok" 1 - "img2sixel accepted missing input"
    exit 0
}

test ! -s "${missing_output}" || {
    echo "not ok" 1 - "img2sixel produced output for missing input"
    exit 0
}

echo "ok" 1 - "missing input rejected without output"
exit 0
