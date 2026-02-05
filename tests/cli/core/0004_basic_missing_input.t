#!/bin/sh
# TAP test ensuring sixel2png rejects missing input paths.

set -eux

CLI_CORE_HELPER_DIR="${TOP_SRCDIR}/tests/lib/sh/cli-core"
. "${CLI_CORE_HELPER_DIR}/cli_core_common.sh"
cli_core_setup "sixel2png-basic"

ensure_converter_available "SIXEL2PNG" "${SIXEL2PNG_PATH}" "sixel2png"



echo "1..1"
set -v

missing_capture=$(make_temp_file "${ARTIFACT_LOCAL_DIR}" "sixel2png-missing")
missing_err=$(make_temp_file "${ARTIFACT_LOCAL_DIR}" "sixel2png-missing-err")
if run_sixel2png -i "${ARTIFACT_LOCAL_DIR}/unknown.six" \
        >"${missing_capture}" 2>"${missing_err}"; then
    cli_core_fail 1 "accepts missing input path"
else
    cli_core_pass 1 "rejects missing input path"
fi
rm -f "${missing_capture}" "${missing_err}"

exit "${status}"
