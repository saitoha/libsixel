#!/bin/sh
# TAP test converting map64.six using explicit stdin/stdout arguments.

set -eux

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
CLI_CORE_HELPER_DIR="${TOP_SRCDIR}/tests/lib/sh/cli-core"
. "${CLI_CORE_HELPER_DIR}/cli_core_common.sh"
cli_core_setup "sixel2png-basic"

ensure_converter_available "SIXEL2PNG" "${SIXEL2PNG_PATH}" "sixel2png"

require_file "${images_dir}/map64.six"

echo "1..1"
set -v

if run_sixel2png - - <"${images_dir}/map64.six" \
        >"${output_dir}/map64-stdin-stdout.png" 2>>"${log_file}"; then
    cli_core_pass 1 "converts map64 with explicit stdin/stdout"
else
    cli_core_fail 1 "map64 stdin/stdout conversion failed"
fi

exit "${status}"
