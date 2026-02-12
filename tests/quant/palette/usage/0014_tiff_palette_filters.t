#!/bin/sh
# Validate TIFF conversion with palette and filter options.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

feature_defined_in_config "HAVE_LIBTIFF" || skip_all "libtiff support is disabled in this build"

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

snake_tiff="${TOP_SRCDIR}/tests/data/inputs/snake_64.tiff"
target_sixel="${ARTIFACT_LOCAL_DIR}/snake-tiff.sixel"

run_img2sixel -Llibtiff! -p200 -8 -scenter -Brgb:0/f/A -h100 -qfull -rhan -dstucki \
    -thls "${snake_tiff}" -o/dev/null || {
    fail 1 "TIFF conversion with palette controls failed"
    exit 0
}

pass 1 "TIFF conversion with palette controls succeeded"

exit 0
