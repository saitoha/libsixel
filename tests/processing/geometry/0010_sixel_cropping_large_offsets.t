#!/bin/sh
# Crop Sixel input with large offsets tolerated.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
. "${TOP_SRCDIR}/tests/lib/sh/common/tap.sh"

status=0

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

snake_six="${images_dir}/snake.six"
target_sixel="${ARTIFACT_LOCAL_DIR}/sixel-crop-offsets.sixel"

if run_img2sixel -c200x200+2000+2000 "${snake_six}" >"${target_sixel}"; then
    pass 1 "Sixel cropping tolerates large offsets"
else
    fail 1 "Sixel cropping with large offsets fails"
fi

exit "${status}"
