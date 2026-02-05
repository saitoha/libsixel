#!/bin/sh
# TAP test verifying sixel2png prints help output.

set -eux

CLI_CORE_HELPER_DIR="${TOP_SRCDIR}/tests/lib/sh/cli-core"
. "${CLI_CORE_HELPER_DIR}/cli_core_common.sh"
cli_core_setup "sixel2png-basic"

ensure_converter_available "SIXEL2PNG" "${SIXEL2PNG_PATH}" "sixel2png"

echo "1..1"
set -v

if run_sixel2png -H >"${output_dir}/help.txt"; then
    cli_core_pass 1 "prints help"
else
    cli_core_fail 1 "help option failed"
fi

exit "${status}"
