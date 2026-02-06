#!/bin/sh
# Ensure indexed PNG scales to a larger width.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
. "${TOP_SRCDIR}/tests/lib/sh/common/tap.sh"

status=0

config_macro_defined HAVE_IMG2SIXEL || skip_all

echo "1..1"
set -v

snake_palette_png="${images_dir}/snake-palette.png"
target_sixel="${ARTIFACT_LOCAL_DIR}/indexed-scale.sixel"

if run_img2sixel -7 -w300 "${snake_palette_png}" \
        >"${target_sixel}"; then
    pass 1 "indexed PNG scales to large width"
else
    fail 1 "indexed PNG scaling fails"
fi

exit "${status}"
