#!/bin/sh
# TAP test ensuring sixel2png rejects invalid thread tokens.

set -eux

CLI_CORE_HELPER_DIR="${TOP_SRCDIR}/tests/lib/sh/cli-core"
. "${CLI_CORE_HELPER_DIR}/cli_core_common.sh"
cli_core_setup "sixel2png-basic"

ensure_converter_available "SIXEL2PNG" "${SIXEL2PNG_PATH}" "sixel2png"



echo "1..1"
set -v

threads_err=$(make_temp_file "${ARTIFACT_LOCAL_DIR}" "sixel2png-threads-err")
if run_sixel2png -= bogus <"${images_dir}/map64.six" \
        >"${ARTIFACT_LOCAL_DIR}/capture.$$" 2>"${threads_err}"; then
    cli_core_fail 1 "accepts invalid thread token"
else
    if grep -F "threads must be a positive integer or 'auto'" \
            "${threads_err}" >/dev/null; then
        cli_core_pass 1 "rejects invalid thread token"
    else
        cli_core_fail 1 "missing invalid thread diagnostic"
    fi
fi
rm -f "${threads_err}" "${ARTIFACT_LOCAL_DIR}/capture.$$"

exit "${status}"
