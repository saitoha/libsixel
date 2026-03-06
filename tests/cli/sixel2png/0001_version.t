#!/bin/sh
# TAP test verifying sixel2png reports version and exits successfully.

set -eux

test "${HAVE_SIXEL2PNG-}" = 1 || {
    printf "1..0 # SKIP sixel2png is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

version_output="${ARTIFACT_LOCAL_DIR}/version.txt"

run_sixel2png -V >"${version_output}" || {
    echo "not ok" 1 - "-V exited with failure"
    exit 0
}

grep -q '^sixel2png ' "${version_output}" || {
    echo "not ok" 1 - "version header missing"
    exit 0
}

echo "ok" 1 - "-V prints version"
exit 0
