#!/bin/sh
# TAP test ensuring sixel2png rejects mixing direct output with dequantize flags.

set -eux

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
CLI_CORE_HELPER_DIR="${TOP_SRCDIR}/tests/lib/sh/cli-core"
. "${CLI_CORE_HELPER_DIR}/cli_core_common.sh"
cli_core_setup "sixel2png-basic"

ensure_converter_available "SIXEL2PNG" "${SIXEL2PNG_PATH}" "sixel2png"

require_file "${images_dir}/snake.six"

echo "1..1"
set -v

direct_err=$(make_temp_file "${tmp_dir}" "sixel2png-direct-err")
if run_sixel2png -D -dk_undither <"${images_dir}/snake.six" \
        >"${tmp_dir}/capture.$$" 2>"${direct_err}"; then
    cli_core_fail 1 "accepts conflicting direct/dequantize flags"
else
    if grep -F "cannot be combined" "${direct_err}" >/dev/null; then
        cli_core_pass 1 "rejects direct/dequantize mix"
    else
        cli_core_fail 1 "missing direct/dequantize diagnostic"
    fi
fi
rm -f "${direct_err}" "${tmp_dir}/capture.$$"

exit "${status}"
