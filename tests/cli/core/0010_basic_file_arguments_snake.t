#!/bin/sh
# TAP test converting snake.six with explicit file arguments.

set -eux

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
CLI_CORE_HELPER_DIR="${TOP_SRCDIR}/tests/lib/sh/cli-core"
. "${CLI_CORE_HELPER_DIR}/cli_core_common.sh"
cli_core_setup "sixel2png-basic"

ensure_converter_available "SIXEL2PNG" "${SIXEL2PNG_PATH}" "sixel2png"

require_file "${images_dir}/snake.six"

echo "1..1"
set -v

if run_sixel2png -i "${images_dir}/snake.six" \
        -o "${output_dir}/snake-file.png" 2>>"${log_file}"; then
    cli_core_pass 1 "converts snake with file arguments"
else
    cli_core_fail 1 "snake file conversion failed"
fi

exit "${status}"
