#!/bin/sh
# TAP test verifying parallel direct conversion matches serial output.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_SIXEL2PNG || skip_all "sixel2png is disabled in this build"

command -v cmp >/dev/null 2>&1 || skip_all "cmp unavailable"

echo "1..1"
set -v

parallel_direct_1="${ARTIFACT_LOCAL_DIR}/parallel-direct-1.png"
parallel_direct_4="${ARTIFACT_LOCAL_DIR}/parallel-direct-4.png"

run_sixel2png --env SIXEL_THREADS=1 -D <"${images_dir}/map64.six" >"${parallel_direct_1}"
run_sixel2png --env SIXEL_THREADS=4 -D <"${images_dir}/map64.six" >"${parallel_direct_4}"

cmp -s "${parallel_direct_1}" "${parallel_direct_4}" || {
    fail 1 "parallel direct diverges"
    exit 0
}

pass 1 "parallel direct matches serial"
exit 0
