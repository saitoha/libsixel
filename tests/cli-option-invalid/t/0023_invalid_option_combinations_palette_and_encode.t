#!/bin/sh
# TAP test ensuring palette and encode flags cannot be combined.

set -euxv

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${script_dir}/../../cli-core/t/cli_core_common.sh"
cli_core_setup "invalid-combinations"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

require_file "${images_dir}/snake.jpg"

echo "1..1"

cli_core_expect_img2sixel_rejection 1 "palette and encode flags conflict" -p16 -e "${images_dir}/snake.jpg"

exit "${status}"
