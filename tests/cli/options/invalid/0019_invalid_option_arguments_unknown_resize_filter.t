#!/bin/sh
# TAP test ensuring img2sixel rejects unknown resize filter values.

set -eux

script_dir=$(CDPATH=; cd "${0%[/\\]*}" && pwd)
CLI_CORE_HELPER_DIR="${TOP_SRCDIR}/tests/lib/sh/cli-core"
. "${CLI_CORE_HELPER_DIR}/cli_core_common.sh"
cli_core_setup "invalid-option-arguments"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo "1..1"
set -v

cli_core_expect_img2sixel_rejection 1 "unknown resize filter" -r invalid_option

exit "${status}"
