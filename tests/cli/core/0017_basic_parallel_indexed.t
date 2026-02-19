#!/bin/sh
# TAP test verifying parallel indexed conversion matches serial output.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_SIXEL2PNG-}" = 1 || {
    printf "1..0 # SKIP sixel2png is disabled in this build\n";
    exit 0
}

command -v cmp >/dev/null 2>&1 || {
    printf "1..0 # SKIP cmp unavailable\n";
    exit 0
}

echo "1..1"
set -v

parallel_indexed_1="${ARTIFACT_LOCAL_DIR}/parallel-indexed-1.png"
parallel_indexed_4="${ARTIFACT_LOCAL_DIR}/parallel-indexed-4.png"
run_sixel2png --env SIXEL_THREADS=1 <"${TOP_SRCDIR}/images/map64.six" >"${parallel_indexed_1}"
run_sixel2png --env SIXEL_THREADS=4 <"${TOP_SRCDIR}/images/map64.six" >"${parallel_indexed_4}"

cmp -s "${parallel_indexed_1}" "${parallel_indexed_4}" >/dev/null || {
    fail 1 "parallel indexed diverges"
    exit 0
}

pass 1 "parallel indexed matches serial"
exit 0
