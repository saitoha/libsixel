#!/bin/sh
# TAP test ensuring img2sixel rejects incompatible options (palette size conflicts with terminal preset).

set -eux

CLI_CORE_HELPER_DIR="${TOP_SRCDIR}/tests/lib/sh/cli-core"
. "${CLI_CORE_HELPER_DIR}/cli_core_common.sh"
cli_core_setup "invalid-option-combinations"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"



echo "1..1"
set -v

cli_core_expect_img2sixel_rejection 1 "palette size conflicts with terminal preset" -p64 -bxterm256 "${images_dir}/snake.png"

exit "${status}"
