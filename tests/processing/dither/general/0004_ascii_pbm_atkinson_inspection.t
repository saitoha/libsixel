#!/bin/sh
# Inspect ASCII PBM with Atkinson diffusion.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

status=0

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

snake_ascii_pbm="${images_dir}/snake-ascii.pbm"
target_txt="${ARTIFACT_LOCAL_DIR}/ascii-pbm-inspection.txt"

if run_img2sixel -I -datkinson "${snake_ascii_pbm}" \
        >"${target_txt}"; then
    pass 1 "ASCII PBM Atkinson inspection works"
else
    fail 1 "ASCII PBM Atkinson inspection fails"
fi

exit "${status}"
