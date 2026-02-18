#!/bin/sh
# TAP test verifying stdin input produces valid PNG on stdout.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_SIXEL2PNG-}" = 1 || skip_all "sixel2png is disabled in this build"

echo "1..1"
set -v

stdout_path="${ARTIFACT_LOCAL_DIR}/stdout.png"

run_sixel2png -i - <"${TOP_SRCDIR}/images/map8.six" >"${stdout_path}" || {
    fail 1 "sixel2png without -o failed"
    exit 0
}

test -s "${stdout_path}" || {
    fail 1 "stdout png missing"
    exit 0
}

expected_header_cksum="3308842558 4"
actual_header_cksum=$(dd bs=1 count=4 if="${stdout_path}" 2>/dev/null | cksum)

test "${actual_header_cksum}" = "${expected_header_cksum}" || {
    fail 1 "stdout png signature is invalid"
    exit 0
}

pass 1 "default stdout PNG produced"
exit 0
