#!/bin/sh
# TAP test producing direct RGBA output with sixel2png.

set -eux

CLI_CORE_HELPER_DIR="${TOP_SRCDIR}/tests/lib/sh/cli-core"
. "${CLI_CORE_HELPER_DIR}/cli_core_common.sh"
cli_core_setup "sixel2png-basic"

ensure_converter_available "SIXEL2PNG" "${SIXEL2PNG_PATH}" "sixel2png"

echo "1..1"
set -v

direct_png="${ARTIFACT_LOCAL_DIR}/snake-direct.png"
if run_sixel2png -D <"${images_dir}/snake.six" >"${direct_png}"; then
    cli_core_pass 1 "produces direct RGBA output"
else
    cli_core_fail 1 "direct RGBA conversion failed"
fi

exit "${status}"
