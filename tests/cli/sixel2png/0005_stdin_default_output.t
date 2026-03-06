#!/bin/sh
# TAP test verifying stdin input produces valid PNG on stdout.

set -eux

test "${HAVE_SIXEL2PNG-}" = 1 || {
    printf "1..0 # SKIP sixel2png is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

stdout_path="${ARTIFACT_LOCAL_DIR}/stdout.png"

run_sixel2png -i - <"${TOP_SRCDIR}/images/map8.six" >"${stdout_path}" || {
    echo "not ok" 1 - "sixel2png without -o failed"
    exit 0
}

test -s "${stdout_path}" || {
    echo "not ok" 1 - "stdout png missing"
    exit 0
}

expected_header_cksum="3308842558 4"
actual_header_cksum=$(dd bs=1 count=4 if="${stdout_path}" 2>/dev/null | cksum)

test "${actual_header_cksum}" = "${expected_header_cksum}" || {
    echo "not ok" 1 - "stdout png signature is invalid"
    exit 0
}

echo "ok" 1 - "default stdout PNG produced"
exit 0
