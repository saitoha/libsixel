#!/bin/sh
# Ensure filename-driven PNG output uses correct header.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build";
    exit 0
}

echo "1..1"
set -v

snake_jpg="${TOP_SRCDIR}/tests/data/inputs/snake_64.jpg"
filename_png="${ARTIFACT_LOCAL_DIR}/snake-filename.png"

run_img2sixel -o "${filename_png}" "${snake_jpg}" || {
    fail 1 "filename-driven PNG conversion failed"
    exit 0
}

expected_header_cksum="3308842558 4"
actual_header_cksum=$(dd bs=1 count=4 if="${filename_png}" 2>/dev/null | cksum)

test "${actual_header_cksum}" = "${expected_header_cksum}" || {
    fail 1 "filename-driven PNG header incorrect"
    exit 0
}

pass 1 "filename-driven PNG output uses PNG header"

exit 0
