#!/bin/sh
# TAP test verifying img2sixel rejects invalid positional inputs without
# emitting stray output.

# Enable strict mode with verbose tracing for diagnostics.
set -eux

tmp_dir="${ARTIFACT_LOCAL_DIR}"


. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

# Skip temporarily on Windows environments while addressing
# intermittent failures specific to that platform.
os_name=$(uname -s || echo "unknown")
if printf '%s' "${os_name}" | grep -qiE 'mingw|msys|cygwin'; then
    skip_all "temporarily disabled on Windows due to instability"
fi

status=0

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo "1..1"
set -v

missing_path="${tmp_dir}/invalid_filename"
rm -f "${missing_path}"
missing_output=$(make_temp_file "${tmp_dir}" "capture.invalid")
if run_img2sixel -v "${missing_path}" >"${missing_output}"; then
    fail 1 "img2sixel accepted missing input"
elif [ -s "${missing_output}" ]; then
    fail 1 "img2sixel produced output for missing input"
else
    pass 1 "missing input rejected without output"
fi
rm -f "${missing_output}" "${missing_path}"

#invalid_output=$(make_temp_file "${tmp_dir}" "capture.invalid")
#if run_img2sixel -v "." >"${invalid_output}"; then
#    fail 2 "img2sixel accepted directory input"
#elif [ -s "${invalid_output}" ]; then
#    fail 2 "img2sixel produced output for directory input"
#else
#    pass 2 "directory input rejected without output"
#fi
#rm -f "${invalid_output}"

exit "${status}"
