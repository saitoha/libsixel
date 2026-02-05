#!/bin/sh
# TAP test verifying invalid decoder arguments surface descriptive errors.

set -eux

output_dir="${ARTIFACT_LOCAL_DIR}"


. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

status=0

ensure_converter_available "SIXEL2PNG" "${SIXEL2PNG_PATH}" "sixel2png"



echo "1..1"
set -v

input_path="${images_dir}/snake.six"


stderr_capture="${output_dir}/stderr.txt"
if run_sixel2png --similarity=invalid "${input_path}" \
        >"${output_dir}/stdout.txt" 2>"${stderr_capture}"; then
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
