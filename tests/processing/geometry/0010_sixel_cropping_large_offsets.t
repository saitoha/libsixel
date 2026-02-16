#!/bin/sh
# Crop Sixel input with large offsets tolerated.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

snake_six="${TOP_SRCDIR}/images/map8.six"
target_sixel="${ARTIFACT_LOCAL_DIR}/sixel-crop-offsets.sixel"

run_img2sixel -c200x200+2000+2000 "${snake_six}" >"${target_sixel}" || {
    fail 1 "Sixel cropping with large offsets fails"
    exit 0
}

pass 1 "Sixel cropping tolerates large offsets"

exit 0
