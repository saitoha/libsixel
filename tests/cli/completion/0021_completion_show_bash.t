#!/bin/sh
# TAP test verifying bash completion output from img2sixel.

set -eux

output_file="${ARTIFACT_LOCAL_DIR}/completion.sh"


. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

status=0

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"



completion_dir="${top_srcdir}/converters/shell-completion"
completion_source="${completion_dir}/bash/img2sixel"


printf '1..1\n'
set -v

IMG2SIXEL_COMPLETION_DIR="${completion_dir}"
export IMG2SIXEL_COMPLETION_DIR

if run_img2sixel -1 bash >"${output_file}"; then
    if grep -F '# bash completion for img2sixel' \
            "${output_file}" >/dev/null 2>&1; then
        pass 1 "bash completion output available"
    else
        fail 1 "missing bash completion header"
    fi
else
    fail 1 "bash completion output failed"
fi

exit "${status}"
