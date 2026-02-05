#!/bin/sh
# TAP test converting map8.six from stdin with sixel2png.

set -eux

CLI_CORE_HELPER_DIR="${TOP_SRCDIR}/tests/lib/sh/cli-core"
. "${CLI_CORE_HELPER_DIR}/cli_core_common.sh"
cli_core_setup "sixel2png-basic"

ensure_converter_available "SIXEL2PNG" "${SIXEL2PNG_PATH}" "sixel2png"

require_file "${images_dir}/map8.six"

echo "1..1"
set -v

if run_sixel2png <"${images_dir}/map8.six" \
        >"${output_dir}/map8-stdin.png"; then
    cli_core_pass 1 "converts map8 from stdin"
else
    cli_core_fail 1 "map8 stdin conversion failed"
fi

exit "${status}"
