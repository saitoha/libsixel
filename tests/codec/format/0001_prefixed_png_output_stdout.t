#!/bin/sh
# Confirm png:- sends the post-SIXEL PNG snapshot to stdout.
# Policy: docs/writers/png.md

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

stdout_png="${ARTIFACT_LOCAL_DIR}/snake-prefixed-stdout.png"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -o "png:-" \
        "${TOP_SRCDIR}/tests/data/inputs/snake_64.jpg" >"${stdout_png}" || {
    echo "not ok" 1 - "png:- conversion failed"
    exit 0
}

expected_header_cksum="3308842558 4"
actual_header_cksum=$(dd bs=1 count=4 if="${stdout_png}" 2>/dev/null | cksum)

test "${actual_header_cksum}" = "${expected_header_cksum}" || {
    echo "not ok" 1 - "png:- did not write PNG data to stdout"
    exit 0
}

echo "ok" 1 - "png:- writes the post-SIXEL PNG snapshot to stdout"
exit 0
