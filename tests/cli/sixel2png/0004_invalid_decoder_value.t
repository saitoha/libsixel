#!/bin/sh
# TAP test verifying invalid decoder arguments surface descriptive errors.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_SIXEL2PNG || skip_all "sixel2png is disabled in this build"

echo "1..1"
set -v

input_path="${images_dir}/map8.six"
stderr_capture="${ARTIFACT_LOCAL_DIR}/stderr.txt"
stdout_capture="${ARTIFACT_LOCAL_DIR}/stdout.txt"

run_sixel2png --similarity=invalid "${input_path}" >"${stdout_capture}"     2>"${stderr_capture}" && {
    fail 1 "invalid similarity should fail"
    exit 0
}

grep -qi "similarity" "${stderr_capture}" >/dev/null 2>&1 && {
    pass 1 "invalid similarity reported"
    exit 0
}

grep -qi "SIXEL_BAD_ARGUMENT" "${stderr_capture}" >/dev/null 2>&1 && {
    pass 1 "invalid similarity reported"
    exit 0
}

fail 1 "error message missing similarity hint"
exit 0
