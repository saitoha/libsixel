#!/bin/sh
# Verify -H prints help text to stdout and exits successfully.

set -eux


printf '1..1\n'
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

out_file="${ARTIFACT_LOCAL_DIR}/lsqa_help_H.stdout"

${SIXEL_RUNTIME-} "${LSQA_PATH}" -H >"${out_file}" || {
    echo "not ok" 1 - "lsqa -H should exit with success"
    exit 0
}

help_marker_found=0
while IFS= read -r line; do
    case "${line}" in
        *"Usage: lsqa"*|*"Options:"*)
            help_marker_found=1
            break
            ;;
    esac
done < "${out_file}"

test "${help_marker_found}" -eq 1 || {
    echo "not ok" 1 - "lsqa -H did not print expected help output"
    exit 0
}

echo "ok" 1 - "lsqa -H printed help text"
exit 0
