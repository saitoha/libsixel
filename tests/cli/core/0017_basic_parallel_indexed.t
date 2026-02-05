#!/bin/sh
# TAP test verifying parallel indexed conversion matches serial output.

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
    cli_core_skip 1 "parallel indexed matches serial" "cmp/diff unavailable"
    exit 0
fi

parallel_indexed_1="${output_dir}/parallel-indexed-1.png"
parallel_indexed_4="${output_dir}/parallel-indexed-4.png"
SIXEL_THREADS=1 run_sixel2png \
    <"${images_dir}/map64.six" \
    >"${parallel_indexed_1}"
SIXEL_THREADS=4 run_sixel2png \
    <"${images_dir}/map64.six" \
    >"${parallel_indexed_4}"

if cli_core_files_identical "${parallel_indexed_1}" "${parallel_indexed_4}"; then
    cli_core_pass 1 "parallel indexed matches serial"
else
    cli_core_fail 1 "parallel indexed diverges"
fi

exit "${status}"
