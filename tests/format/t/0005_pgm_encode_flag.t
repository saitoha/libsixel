#!/bin/sh
# Confirm PGM encode flag cooperates with palette auto-selection.
set -eux

conversion_common_path=$(CDPATH=; cd "$(dirname "$0")/.." && pwd)/../lib/sh/conversion/common.sh
. "${conversion_common_path}"

test_name=$(basename "$0")
setup_conversion_env "${test_name}"

status=0

ensure_img2sixel_available

echo "1..1"

snake_pgm="${images_dir}/snake.pgm"
require_file "${snake_pgm}"

if run_img2sixel -8 -qauto -thls -e "${snake_pgm}" -o/dev/null \
    2>>"${log_file}"; then
    pass 1 "PGM encode flag cooperates with palette auto-selection"
else
    fail 1 "PGM encode flag failed"
fi

exit "${status}"
