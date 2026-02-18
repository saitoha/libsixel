#!/bin/sh
# Inspect ASCII PBM with Atkinson diffusion.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

snake_ascii_pbm="${TOP_SRCDIR}/images/snake-ascii.pbm"
target_txt="${ARTIFACT_LOCAL_DIR}/ascii-pbm-inspection.txt"

run_img2sixel -I -datkinson "${snake_ascii_pbm}" \
        >"${target_txt}" || {
    fail 1 "ASCII PBM Atkinson inspection fails"
    exit 0
}

pass 1 "ASCII PBM Atkinson inspection works"

exit 0
