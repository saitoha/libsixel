#!/bin/sh
# TAP test ensuring img2sixel rejects missing palette file.

set -eux

CLI_CORE_HELPER_DIR="${TOP_SRCDIR}/tests/lib/sh/cli-core"
. "${CLI_CORE_HELPER_DIR}/cli_core_common.sh"
cli_core_setup "invalid-option-arguments"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

require_file "${images_dir}/snake.jpg"

echo "1..1"
set -v

cli_core_expect_img2sixel_rejection 1 "missing palette file" -m "${tmp_dir}/invalid_filename" "${images_dir}/snake.jpg"

exit "${status}"
