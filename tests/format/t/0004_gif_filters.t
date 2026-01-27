#!/bin/sh
# Validate GIF conversion with scaling and filters.
set -eux

conversion_common_path=$(CDPATH=; cd "$(dirname "$0")/.." && pwd)/../lib/sh/conversion/common.sh
. "${conversion_common_path}"

test_name=$(basename "$0")
setup_conversion_env "${test_name}"

status=0

ensure_img2sixel_available

echo "1..1"
set -v

snake_gif="${images_dir}/snake.gif"
target_sixel="${tmp_dir}/snake-gif.sixel"

require_file "${snake_gif}"

if run_img2sixel -w105% -h100 -B"#000000000" -rne <"${snake_gif}" \
    >"${target_sixel}" 2>>"${log_file}"; then
    pass 1 "GIF conversion with filters succeeded"
else
    fail 1 "GIF conversion with filters failed"
fi

exit "${status}"
