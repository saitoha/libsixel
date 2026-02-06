#!/bin/sh
# TAP test verifying CLI thread override matches environment-based output.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_SIXEL2PNG || skip_all "sixel2png is disabled in this build"

echo "1..1"
set -v

comparator_cmd=""
if command -v cmp >/dev/null 2>&1; then
    comparator_cmd="cmp -s"
elif command -v diff >/dev/null 2>&1; then
    comparator_cmd="diff -q"
else
    skip_all "parallel indexed matches serial" "cmp/diff unavailable"
    exit 0
fi

parallel_direct_4="${ARTIFACT_LOCAL_DIR}/parallel-direct-4.png"
parallel_direct_cli="${ARTIFACT_LOCAL_DIR}/parallel-direct-cli.png"
SIXEL_THREADS=4 run_sixel2png -D <"${images_dir}/map64.six" >"${parallel_direct_4}"
SIXEL_THREADS=1 run_sixel2png -D <"${images_dir}/map64.six" >"${parallel_direct_cli}"

${comparator_cmd} "${parallel_direct_cli}" "${parallel_direct_4}" || {
    fail 1 "CLI thread override diverges"
    exit 0
}

pass 1 "CLI thread override matches env"
exit 0
