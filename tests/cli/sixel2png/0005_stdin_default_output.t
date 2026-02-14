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

run_sixel2png -i - <"${images_dir}/map8.six" >"${stdout_path}" 2>"${stderr_path}" || {
    fail 1 "sixel2png without -o failed"
    exit 0
}

test -s "${stdout_path}" || {
    fail 1 "stdout png missing"
    exit 0
}

signature_hex="$(dd if="${stdout_path}" bs=1 count=5 2>/dev/null | \
    LC_ALL=C od -An -tx1 | awk '{gsub(/[[:space:]]/, ""); printf "%s", $0} END {print ""}')"

test "${signature_hex}" = "89504e470d" || {
    fail 1 "stdout png signature is invalid"
    exit 0
}

pass 1 "default stdout PNG produced"
exit 0
