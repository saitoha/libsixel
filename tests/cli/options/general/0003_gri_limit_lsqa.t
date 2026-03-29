#!/bin/sh
# TAP test validating --gri-limit preserves output quality and determinism.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
output_plain="${ARTIFACT_LOCAL_DIR}/plain.six"
output_limited="${ARTIFACT_LOCAL_DIR}/limited.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -=1 -o "${output_plain}" "${input_image}" || {
    echo "not ok" 1 - "img2sixel failed"
    exit 0
}
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -=1 --gri-limit -o "${output_limited}" "${input_image}" || {
    echo "not ok" 1 - "img2sixel failed"
    exit 0
}

lsqa_message=$(
    set +xv
    ${SIXEL_RUNTIME-} "${LSQA_PATH}" -b "MS-SSIM:1.00" "${output_plain}" "${output_limited}" > /dev/null 2>&1
) || lsqa_run_status=$?

test "${lsqa_run_status-0}" -eq 0 && {
    echo "ok" 1 - "gri-limit deterministic output matches"
    exit 0
}

test "${lsqa_run_status-0}" -eq 5 && {
    echo "not ok" 1 - "${lsqa_message}"
    exit 0
}

echo "not ok" 1 - "gri-limit deterministic output mismatch"
exit 0
