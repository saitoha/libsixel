#!/bin/sh
# Ensure filename-driven PNG output uses correct header.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

snake_jpg="${TOP_SRCDIR}/tests/data/inputs/snake_64.jpg"
filename_png="${ARTIFACT_LOCAL_DIR}/snake-filename.png"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -o "${filename_png}" "${snake_jpg}" || {
    echo "not ok" 1 - "filename-driven PNG conversion failed"
    exit 0
}

expected_header_cksum="3308842558 4"
actual_header_cksum=$(dd bs=1 count=4 if="${filename_png}" 2>/dev/null | cksum)

test "${actual_header_cksum}" = "${expected_header_cksum}" || {
    echo "not ok" 1 - "filename-driven PNG header incorrect"
    exit 0
}

echo "ok" 1 - "filename-driven PNG output uses PNG header"

exit 0
