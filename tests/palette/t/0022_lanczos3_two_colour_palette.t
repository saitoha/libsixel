#!/bin/sh
# Scale with Lanczos3 filter using a two-colour palette.
set -eux

conversion_common_path=$(CDPATH=; cd "$(dirname "$0")/.." && pwd)/../conversion/common.sh
. "${conversion_common_path}"

test_name=$(basename "$0")
setup_conversion_env "${test_name}"

status=0

ensure_img2sixel_available

echo "1..1"

snake_jpg="${images_dir}/snake.jpg"
target_sixel="${output_dir}/lanczos3-two-colour.sixel"

require_file "${snake_jpg}"

if run_img2sixel -p 2 -h100 -wauto -rlanczos3 "${snake_jpg}" \
        >"${target_sixel}" 2>>"${log_file}"; then
    pass 1 "Lanczos3 scaling with two-colour palette works"
else
    fail 1 "Lanczos3 scaling with two-colour palette fails"
fi

exit "${status}"
