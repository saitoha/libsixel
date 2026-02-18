#!/bin/sh
# TAP test verifying CLI thread override matches environment-based output.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_SIXEL2PNG-}" = 1 || skip_all "sixel2png is disabled in this build"

command -v cmp >/dev/null 2>&1 || skip_all "cmp unavailable"

echo "1..1"
set -v

parallel_direct_4="${ARTIFACT_LOCAL_DIR}/parallel-direct-4.png"
parallel_direct_cli="${ARTIFACT_LOCAL_DIR}/parallel-direct-cli.png"
run_sixel2png --env SIXEL_THREADS=4 -D <"${TOP_SRCDIR}/images/map64.six" >"${parallel_direct_4}"
run_sixel2png --env SIXEL_THREADS=1 -D <"${TOP_SRCDIR}/images/map64.six" >"${parallel_direct_cli}"

cmp -s "${parallel_direct_cli}" "${parallel_direct_4}" || {
    fail 1 "CLI thread override diverges"
    exit 0
}

pass 1 "CLI thread override matches env"
exit 0
