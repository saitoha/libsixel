#!/bin/sh
# TAP test verifying invalid decoder arguments surface descriptive errors.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

status=0

config_macro_defined HAVE_SIXEL2PNG || skip_all "sixel2png is disabled in this build"

echo "1..1"
set -v

input_path="${images_dir}/map8.six"


stderr_capture="${ARTIFACT_LOCAL_DIR}/stderr.txt"
if run_sixel2png --similarity=invalid "${input_path}" \
        >"${ARTIFACT_LOCAL_DIR}/stdout.txt" 2>"${stderr_capture}"; then
    fail 1 "invalid similarity should fail"
else
    if grep -qi -- "similarity" "${stderr_capture}" \
            || grep -qi -- "SIXEL_BAD_ARGUMENT" "${stderr_capture}"; then
        pass 1 "invalid similarity reported"
    else
        fail 1 "error message missing similarity hint"
    fi
fi

exit "${status}"
