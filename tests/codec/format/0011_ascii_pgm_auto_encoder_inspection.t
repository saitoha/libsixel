#!/bin/sh
# Inspect ASCII PGM using automatic encoder selection.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

snake_ascii_pgm="${TOP_SRCDIR}/images/snake-ascii.pgm"
target_txt="${ARTIFACT_LOCAL_DIR}/ascii-pgm-inspection.txt"

run_img2sixel -I -Eauto "${snake_ascii_pgm}" >"${target_txt}" || {
    echo "not ok" 1 "ASCII PGM auto encoder inspection fails"
    exit 0
}

echo "ok" 1 "ASCII PGM auto encoder inspection works"

exit 0
