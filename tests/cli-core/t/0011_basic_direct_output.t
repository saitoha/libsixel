#!/bin/sh
# TAP test producing direct RGBA output with sixel2png.

set -euxv

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
CLI_CORE_HELPER_DIR="${script_dir}/../../lib/sh/cli-core"
. "${CLI_CORE_HELPER_DIR}/cli_core_common.sh"
cli_core_setup "sixel2png-basic"

ensure_converter_available "SIXEL2PNG" "${SIXEL2PNG_PATH}" "sixel2png"

require_file "${images_dir}/snake.six"

echo "1..1"

direct_png="${output_dir}/snake-direct.png"
if run_sixel2png -D <"${images_dir}/snake.six" \
        >"${direct_png}" 2>>"${log_file}"; then
    cli_core_pass 1 "produces direct RGBA output"
else
    cli_core_fail 1 "direct RGBA conversion failed"
fi

exit "${status}"
