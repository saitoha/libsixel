#!/bin/sh
# Emit monochrome frame and append trailer marker.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
. "${TOP_SRCDIR}/tests/lib/sh/common/tap.sh"

status=0

config_macro_defined HAVE_IMG2SIXEL || skip_all

echo "1..1"
set -v

snake_jpg="${TOP_SRCDIR}/tests/data/inputs/snake_64.jpg"
target_sixel="${ARTIFACT_LOCAL_DIR}/monochrome-frame.sixel"
trailer_txt="${ARTIFACT_LOCAL_DIR}/monochrome-frame-trailer.txt"

if run_img2sixel -p 1 -h100 -n1 "${snake_jpg}" >"${target_sixel}"; then
    printf '\033[*1z' >"${trailer_txt}"
    pass 1 "monochrome frame with trailer succeeds"
else
    fail 1 "monochrome frame with trailer fails"
fi

exit "${status}"
