#!/bin/sh
# Inspect ASCII PGM using automatic encoder selection.
set -eux

conversion_common_path="${TOP_SRCDIR}/tests/lib/sh/conversion/common.sh"
. "${conversion_common_path}"

status=0

ensure_img2sixel_available

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
