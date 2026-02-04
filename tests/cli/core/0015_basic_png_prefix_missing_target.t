#!/bin/sh
# TAP test ensuring an empty png: prefix is rejected.

set -eux

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
CLI_CORE_HELPER_DIR="${TOP_SRCDIR}/tests/lib/sh/cli-core"
. "${CLI_CORE_HELPER_DIR}/cli_core_common.sh"
cli_core_setup "sixel2png-basic"

ensure_converter_available "SIXEL2PNG" "${SIXEL2PNG_PATH}" "sixel2png"

require_file "${images_dir}/snake.six"

echo "1..1"
set -v

png_err=$(make_temp_file "${tmp_dir}" "sixel2png-png-err")
if run_sixel2png -o "png:" <"${images_dir}/snake.six" \
        >"${tmp_dir}/capture.$$" 2>"${png_err}"; then
    cli_core_fail 1 "accepts empty png: prefix"
else
    if grep -F 'missing target after the "png:" prefix' "${png_err}" >/dev/null; then
        cli_core_pass 1 "rejects empty png prefix"
    else
        cli_core_fail 1 "missing png prefix diagnostic"
    fi
fi
rm -f "${png_err}" "${tmp_dir}/capture.$$"

exit "${status}"
