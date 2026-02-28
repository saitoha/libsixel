#!/bin/sh
# TAP test: coregraphics multisize ICO output does not start with ESC [ H.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_COREGRAPHICS-}" = 1 || {
    printf "1..0 # SKIP coregraphics support is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

image_path="${TOP_SRCDIR}/tests/data/inputs/formats/snake-ico-multisize.ico"
output_sixel="${ARTIFACT_LOCAL_DIR}/coregraphics_ico_multisize_prefix.six"

run_img2sixel -L coregraphics! "${image_path}" >"${output_sixel}" || {
    echo "not ok" 1 "coregraphics failed to decode multi-size ICO input"
    exit 0
}

prefix_hex=$(xxd -l 3 -p "${output_sixel}")
test "${prefix_hex}" = "1b5b48" && {
    echo "not ok" 1 "multi-size ICO output unexpectedly starts with ESC [ H"
    exit 0
}

echo "ok" 1 "coregraphics multisize ICO output does not start with ESC [ H"
exit 0
