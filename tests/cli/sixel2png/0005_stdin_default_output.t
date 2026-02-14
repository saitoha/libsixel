#!/bin/sh
# TAP test verifying stdin input produces valid PNG on stdout.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_SIXEL2PNG || skip_all "sixel2png is disabled in this build"

echo "1..1"
set -v

stdout_path="${ARTIFACT_LOCAL_DIR}/stdout.png"
stderr_path="${ARTIFACT_LOCAL_DIR}/stderr.txt"

: >"${stdout_path}"
: >"${stderr_path}"

run_sixel2png -i - <"${TOP_SRCDIR}/images/map8.six" >"${stdout_path}" 2>"${stderr_path}" || {
    fail 1 "sixel2png without -o failed"
    exit 0
}

test -s "${stdout_path}" || {
    fail 1 "stdout png missing"
    exit 0
}

expected_signature=$(printf '%b' "\211PNG")
actual_signature=$(dd if="${stdout_path}" bs=1 count=4 2>/dev/null | awk 'BEGIN { RS = "\0"; ORS = "" } { print $0 }')

[ "${actual_signature}" = "${expected_signature}" ] || {
    fail 1 "stdout png signature is invalid"
    exit 0
}

pass 1 "default stdout PNG produced"
exit 0
