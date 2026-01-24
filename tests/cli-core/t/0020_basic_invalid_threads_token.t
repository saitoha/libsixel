#!/bin/sh
# TAP test ensuring sixel2png rejects invalid thread tokens.

set -euxv

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
CLI_CORE_HELPER_DIR="${script_dir}/../../lib/sh/cli-core"
. "${CLI_CORE_HELPER_DIR}/cli_core_common.sh"
cli_core_setup "sixel2png-basic"

ensure_converter_available "SIXEL2PNG" "${SIXEL2PNG_PATH}" "sixel2png"

require_file "${images_dir}/map64.six"

echo "1..1"

threads_err=$(make_temp_file "${tmp_dir}" "sixel2png-threads-err")
if run_sixel2png -= bogus <"${images_dir}/map64.six" \
        >"${tmp_dir}/capture.$$" 2>"${threads_err}"; then
    cli_core_fail 1 "accepts invalid thread token"
else
    if grep -F "threads must be a positive integer or 'auto'" \
            "${threads_err}" >/dev/null; then
        cli_core_pass 1 "rejects invalid thread token"
    else
        cli_core_fail 1 "missing invalid thread diagnostic"
    fi
fi
rm -f "${threads_err}" "${tmp_dir}/capture.$$"

exit "${status}"
