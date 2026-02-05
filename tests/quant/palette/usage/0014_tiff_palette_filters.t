#!/bin/sh
# Validate TIFF conversion with palette and filter options.
set -eux

conversion_common_path="${TOP_SRCDIR}/tests/lib/sh/conversion/common.sh"
. "${conversion_common_path}"

status=0

if ! feature_defined_in_config "HAVE_LIBTIFF"; then
    skip_all "libtiff support is disabled in this build"
fi

ensure_img2sixel_available

echo "1..1"
set -v

snake_tiff="${TOP_SRCDIR}/tests/data/inputs/snake_64.tiff"
target_sixel="${ARTIFACT_LOCAL_DIR}/snake-tiff.sixel"

if run_img2sixel -Llibtiff! -p200 -8 -scenter -Brgb:0/f/A -h100 -qfull -rhan -dstucki \
    -thls "${snake_tiff}" -o/dev/null; then
    pass 1 "TIFF conversion with palette controls succeeded"
else
    fail 1 "TIFF conversion with palette controls failed"
fi

exit "${status}"
