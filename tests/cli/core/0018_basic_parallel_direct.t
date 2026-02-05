#!/bin/sh
# TAP test verifying parallel direct conversion matches serial output.

set -eux

CLI_CORE_HELPER_DIR="${TOP_SRCDIR}/tests/lib/sh/cli-core"
. "${CLI_CORE_HELPER_DIR}/cli_core_common.sh"
cli_core_setup "sixel2png-basic"

ensure_converter_available "SIXEL2PNG" "${SIXEL2PNG_PATH}" "sixel2png"



echo "1..1"
set -v

cli_core_pick_comparator
if [ -z "${comparator_cmd}" ]; then
    cli_core_skip 1 "parallel direct matches serial" "cmp/diff unavailable"
    exit 0
fi

parallel_direct_1="${ARTIFACT_LOCAL_DIR}/parallel-direct-1.png"
parallel_direct_4="${ARTIFACT_LOCAL_DIR}/parallel-direct-4.png"
SIXEL_THREADS=1 run_sixel2png -D \
    <"${images_dir}/map64.six" \
    >"${parallel_direct_1}"
SIXEL_THREADS=4 run_sixel2png -D \
    <"${images_dir}/map64.six" \
    >"${parallel_direct_4}"

if cli_core_files_identical "${parallel_direct_1}" "${parallel_direct_4}"; then
    cli_core_pass 1 "parallel direct matches serial"
else
    cli_core_fail 1 "parallel direct diverges"
fi

exit "${status}"
