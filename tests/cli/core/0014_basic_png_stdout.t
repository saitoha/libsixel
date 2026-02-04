#!/bin/sh
# TAP test verifying png:- writes PNG data to stdout.

set -eux

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
CLI_CORE_HELPER_DIR="${TOP_SRCDIR}/tests/lib/sh/cli-core"
. "${CLI_CORE_HELPER_DIR}/cli_core_common.sh"
cli_core_setup "sixel2png-basic"

ensure_converter_available "SIXEL2PNG" "${SIXEL2PNG_PATH}" "sixel2png"

require_file "${images_dir}/snake.six"

echo "1..1"
set -v

png_stdout="${output_dir}/png-stdout.png"
if run_sixel2png -o "png:-" <"${images_dir}/snake.six" \
        >"${png_stdout}" 2>>"${log_file}"; then
    if [ -s "${png_stdout}" ]; then
        cli_core_pass 1 "png:- writes to stdout"
    else
        cli_core_fail 1 "png:- produced empty output"
    fi
else
    cli_core_fail 1 "png:- command failed"
fi

exit "${status}"
