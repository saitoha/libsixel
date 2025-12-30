#!/bin/sh
# TAP test confirming assessment spool persists across multiple inputs.

# Enable strict mode with verbose tracing for diagnostics.
set -euxv

test_name=$(basename "$0")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${test_name}"
log_file="${artifact_dir}/assessment-spool.log"

mkdir -p "${artifact_dir}"

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${script_dir}/../../common/t/0001_converters_common.t"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

require_file "${images_dir}/snake.jpg"
require_file "${images_dir}/map8.png"

status=0
case_id=1

pass() {
    printf 'ok %s - %s\n' "$1" "$2"
}

fail() {
    printf 'not ok %s - %s\n' "$1" "$2"
    status=1
}

echo "1..1"

quality_err="${artifact_dir}/assessment-quality.err"
quality_out="${artifact_dir}/assessment-quality.out"
rm -f "${quality_err}" "${quality_out}"

if run_img2sixel -a quality "${images_dir}/snake.jpg" \
        "${images_dir}/map8.png" \
        >"${quality_out}" 2>"${quality_err}"; then
    fail ${case_id} "quality assessment unexpectedly succeeded"
else
    if [ -s "${quality_out}" ]; then
        fail ${case_id} "quality assessment produced output"
        printf '--- stdout ---\n' >>"${log_file}"
        cat "${quality_out}" >>"${log_file}" 2>/dev/null || :
    else
        pass ${case_id} "assessment spool reported failure without output"
    fi
fi

exit "${status}"
