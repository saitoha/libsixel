#!/bin/sh
# TAP test converting snake.six from stdin with sixel2png.

set -eux

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
CLI_CORE_HELPER_DIR="${script_dir}/../../lib/sh/cli-core"
. "${CLI_CORE_HELPER_DIR}/cli_core_common.sh"
cli_core_setup "sixel2png-basic"

ensure_converter_available "SIXEL2PNG" "${SIXEL2PNG_PATH}" "sixel2png"

require_file "${images_dir}/snake.six"

echo "1..1"
set -v

if run_sixel2png <"${images_dir}/snake.six" \
        >"${output_dir}/snake-stdin.png" 2>>"${log_file}"; then
    cli_core_pass 1 "converts snake from stdin"
else
    cli_core_fail 1 "snake stdin conversion failed"
fi

exit "${status}"
