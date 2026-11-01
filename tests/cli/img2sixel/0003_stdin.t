#!/bin/sh
# TAP test verifying img2sixel rejects non-image stdin data without output.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

output_file="${ARTIFACT_LOCAL_DIR}/capture.stdin"
input_file="${ARTIFACT_LOCAL_DIR}/invalid.stdin"

printf "a\n" >"${input_file}"

# Feed stdin through a regular redirection instead of a pipeline.  Some
# POSIX-on-native layers keep the writer side of a short pipeline observable
# long enough to confuse this negative-input path, while the program behavior
# under test is only that "-" reads from stdin and rejects invalid image data.
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" - <"${input_file}" >"${output_file}" && :

test ! -s "${output_file}" || {
    echo "not ok" 1 - "img2sixel produced output for invalid stdin"
    exit 0
}

echo "ok" 1 - "invalid stdin rejected without output"
exit 0
