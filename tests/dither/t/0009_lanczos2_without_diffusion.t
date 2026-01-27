#!/bin/sh
# Scale with Lanczos2 filter while disabling diffusion.
set -eux

conversion_common_path=$(CDPATH=; cd "$(dirname "$0")/.." && pwd)/../lib/sh/conversion/common.sh
. "${conversion_common_path}"

test_name=$(basename "$0")
setup_conversion_env "${test_name}"

status=0

ensure_img2sixel_available

echo "1..1"
set -v

snake_jpg="${images_dir}/snake.jpg"
target_sixel="${output_dir}/lanczos2-no-diffusion.sixel"

require_file "${snake_jpg}"

if run_img2sixel -v -p 8 -h200 -fnorm -rlanczos2 -dnone \
        "${snake_jpg}" >"${target_sixel}" 2>>"${log_file}"; then
    pass 1 "Lanczos2 scaling without diffusion succeeds"
else
    fail 1 "Lanczos2 scaling without diffusion fails"
fi

exit "${status}"
