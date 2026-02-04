#!/bin/sh
# TAP test ensuring img2sixel rejects overlong background specifications.

set -eux

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
CLI_CORE_HELPER_DIR="${TOP_SRCDIR}/tests/lib/sh/cli-core"
. "${CLI_CORE_HELPER_DIR}/cli_core_common.sh"
cli_core_setup "invalid-option-arguments"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

require_file "${images_dir}/map8.png"

echo "1..1"
set -v

cli_core_expect_img2sixel_rejection 1 "overlong background specification" -B '#0000000000000' "${images_dir}/map8.png"

exit "${status}"
