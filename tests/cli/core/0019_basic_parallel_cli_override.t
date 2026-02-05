#!/bin/sh
# TAP test verifying CLI thread override matches environment-based output.

set -eux

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
CLI_CORE_HELPER_DIR="${TOP_SRCDIR}/tests/lib/sh/cli-core"
. "${CLI_CORE_HELPER_DIR}/cli_core_common.sh"
cli_core_setup "sixel2png-basic"

ensure_converter_available "SIXEL2PNG" "${SIXEL2PNG_PATH}" "sixel2png"

require_file "${images_dir}/map64.six"

echo "1..1"
set -v

cli_core_pick_comparator
if [ -z "${comparator_cmd}" ]; then
    cli_core_skip 1 "CLI thread override matches env" "cmp/diff unavailable"
    exit 0
fi

parallel_direct_4="${output_dir}/parallel-direct-4.png"
parallel_direct_cli="${output_dir}/parallel-direct-cli.png"
SIXEL_THREADS=4 run_sixel2png -D \
    <"${images_dir}/map64.six" \
    >"${parallel_direct_4}"
SIXEL_THREADS=1 run_sixel2png -D \
    <"${images_dir}/map64.six" \
    >"${parallel_direct_cli}"

if cli_core_files_identical "${parallel_direct_cli}" "${parallel_direct_4}"; then
    cli_core_pass 1 "CLI thread override matches env"
else
    cli_core_fail 1 "CLI thread override diverges"
fi

exit "${status}"
