#!/bin/sh
# TAP test ensuring inspect and palette options conflict as expected.

set -eux

CLI_CORE_HELPER_DIR="${TOP_SRCDIR}/tests/lib/sh/cli-core"
. "${CLI_CORE_HELPER_DIR}/cli_core_common.sh"
cli_core_setup "invalid-combinations"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo "1..1"
set -v

cli_core_expect_img2sixel_rejection 1 "inspect and palette options conflict" \
    -I -p8 "${TOP_SRCDIR}/tests/data/inputs/snake_64.png"

exit "${status}"
