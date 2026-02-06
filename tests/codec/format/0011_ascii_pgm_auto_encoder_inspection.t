#!/bin/sh
# Inspect ASCII PGM using automatic encoder selection.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
. "${TOP_SRCDIR}/tests/lib/sh/common/tap.sh"

status=0

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

snake_ascii_pgm="${images_dir}/snake-ascii.pgm"
target_txt="${ARTIFACT_LOCAL_DIR}/ascii-pgm-inspection.txt"

if run_img2sixel -I -Eauto "${snake_ascii_pgm}" >"${target_txt}"; then
    pass 1 "ASCII PGM auto encoder inspection works"
else
    fail 1 "ASCII PGM auto encoder inspection fails"
fi

exit "${status}"
