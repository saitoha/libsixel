#!/bin/sh
# TAP test ensuring img2sixel rejects invalid colour space indices.

set -eux

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
CLI_CORE_HELPER_DIR="${script_dir}/../../lib/sh/cli-core"
. "${CLI_CORE_HELPER_DIR}/cli_core_common.sh"
cli_core_setup "invalid-option-arguments"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

require_file "${images_dir}/snake.png"

echo "1..1"
set -v

cli_core_expect_img2sixel_rejection 1 "invalid colour space index" -I -C0 "${images_dir}/snake.png"

exit "${status}"
