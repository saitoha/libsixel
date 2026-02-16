#!/bin/sh
# Inspect ASCII PGM using automatic encoder selection.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

snake_ascii_pgm="${TOP_SRCDIR}/images/snake-ascii.pgm"
target_txt="${ARTIFACT_LOCAL_DIR}/ascii-pgm-inspection.txt"

run_img2sixel -I -Eauto "${snake_ascii_pgm}" >"${target_txt}" || {
    fail 1 "ASCII PGM auto encoder inspection fails"
    exit 0
}

pass 1 "ASCII PGM auto encoder inspection works"

exit 0
