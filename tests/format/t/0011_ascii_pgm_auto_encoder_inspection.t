#!/bin/sh
# Inspect ASCII PGM using automatic encoder selection.
set -eux

conversion_common_path=$(CDPATH=; cd "$(dirname "$0")/.." && pwd)/../lib/sh/conversion/common.sh
. "${conversion_common_path}"

test_name=$(basename "$0")
setup_conversion_env "${test_name}"

status=0

ensure_img2sixel_available

echo "1..1"

snake_ascii_pgm="${images_dir}/snake-ascii.pgm"
target_txt="${output_dir}/ascii-pgm-inspection.txt"

require_file "${snake_ascii_pgm}"

if run_img2sixel -I -Eauto "${snake_ascii_pgm}" \
        >"${target_txt}" 2>>"${log_file}"; then
    pass 1 "ASCII PGM auto encoder inspection works"
else
    fail 1 "ASCII PGM auto encoder inspection fails"
fi

exit "${status}"
