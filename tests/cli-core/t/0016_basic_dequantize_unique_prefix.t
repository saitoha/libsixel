#!/bin/sh
# TAP test confirming sixel2png accepts an unambiguous dequantize prefix.

set -eux

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
CLI_CORE_HELPER_DIR="${script_dir}/../../lib/sh/cli-core"
. "${CLI_CORE_HELPER_DIR}/cli_core_common.sh"
cli_core_setup "sixel2png-basic"

ensure_converter_available "SIXEL2PNG" "${SIXEL2PNG_PATH}" "sixel2png"

require_file "${images_dir}/snake.six"

echo "1..1"
set -v

ambiguous_err=$(make_temp_file "${tmp_dir}" "sixel2png-ambiguous")
set +xv
if run_sixel2png -dk_ <"${images_dir}/snake.six" \
        >"${output_dir}/dequantize-short.png" 2>"${ambiguous_err}"; then
    set -xv
    if [ -s "${output_dir}/dequantize-short.png" ]; then
        cli_core_pass 1 "accepts unique dequantize prefix"
    else
        cli_core_fail 1 "unexpected diagnostics for -dk_"
    fi
else
    set -xv
    cli_core_fail 1 "unique dequantize prefix rejected"
fi
rm -f "${ambiguous_err}"

exit "${status}"
