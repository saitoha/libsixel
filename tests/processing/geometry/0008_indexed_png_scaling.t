#!/bin/sh
# Ensure indexed PNG scales to a larger width.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

snake_palette_png="${images_dir}/snake-palette.png"
target_sixel="${ARTIFACT_LOCAL_DIR}/indexed-scale.sixel"

run_img2sixel -7 -w300 "${snake_palette_png}" \
        >"${target_sixel}" || {
    fail 1 "indexed PNG scaling fails"
    exit 0
}

pass 1 "indexed PNG scales to large width"

exit 0
