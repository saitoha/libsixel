#!/bin/sh
# TAP test ensuring img2sixel rejects incompatible options (palette size conflicts with terminal preset).

set -eux

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
CLI_CORE_HELPER_DIR="${script_dir}/../../lib/sh/cli-core"
. "${CLI_CORE_HELPER_DIR}/cli_core_common.sh"
cli_core_setup "invalid-option-combinations"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

require_file "${images_dir}/snake.png"

echo "1..1"
set -v

cli_core_expect_img2sixel_rejection 1 "palette size conflicts with terminal preset" -p64 -bxterm256 "${images_dir}/snake.png"

exit "${status}"
