#!/bin/sh
# TAP test verifying parallel indexed conversion matches serial output.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_SIXEL2PNG || skip_all "sixel2png is disabled in this build"

comparator_cmd=""
if command -v cmp >/dev/null 2>&1; then
    comparator_cmd="cmp -s"
elif command -v diff >/dev/null 2>&1; then
    comparator_cmd="diff -q"
else
    skip_all "parallel indexed matches serial" "cmp/diff unavailable"
    exit 0
fi

echo "1..1"
set -v

parallel_indexed_1="${ARTIFACT_LOCAL_DIR}/parallel-indexed-1.png"
parallel_indexed_4="${ARTIFACT_LOCAL_DIR}/parallel-indexed-4.png"
run_sixel2png --env SIXEL_THREADS=1 <"${images_dir}/map64.six" >"${parallel_indexed_1}"
run_sixel2png --env SIXEL_THREADS=4 <"${images_dir}/map64.six" >"${parallel_indexed_4}"

${comparator_cmd} "${parallel_indexed_1}" "${parallel_indexed_4}" >/dev/null || {
    fail 1 "parallel indexed diverges"
    exit 0
}

pass 1 "parallel indexed matches serial"
exit 0
