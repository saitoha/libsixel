#!/bin/sh
# Ensure filename-driven PNG output uses correct header.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

snake_jpg="${TOP_SRCDIR}/tests/data/inputs/snake_64.jpg"
filename_png="${ARTIFACT_LOCAL_DIR}/snake-filename.png"

run_img2sixel -o "${filename_png}" "${snake_jpg}" || {
    fail 1 "filename-driven PNG conversion failed"
    exit 0
}

expected_header=$(printf '%b' "\211PNG\r\n\032\n")
actual_header=$(dd if="${filename_png}" bs=1 count=8 2>/dev/null     | awk 'BEGIN { RS = "\0"; ORS = "" } { print $0 }')

[ "${actual_header}" = "${expected_header}" ] || {
    fail 1 "filename-driven PNG header incorrect"
    exit 0
}

pass 1 "filename-driven PNG output uses PNG header"

exit 0
