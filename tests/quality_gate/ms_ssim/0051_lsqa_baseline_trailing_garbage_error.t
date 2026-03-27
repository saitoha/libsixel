#!/bin/sh
# Verify parse error when baseline value has trailing garbage characters.

set -eux


printf '1..1\n'
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

image_ref="${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp"
err_file="${ARTIFACT_LOCAL_DIR}/lsqa_baseline_trailing_garbage.err"

set +e
${SIXEL_RUNTIME-} "${LSQA_PATH}" -b "MS-SSIM:1.0x" "${image_ref}" >"/dev/null" 2>"${err_file}"
status=$?
set -e

test "${status}" -eq 2 || {
    echo "not ok" 1 - "baseline trailing garbage was not rejected as expected"
    exit 0
}

grep -q "Unexpected characters in value" "${err_file}" || {
    echo "not ok" 1 - "baseline trailing garbage was not rejected as expected"
    exit 0
}

echo "ok" 1 - "baseline trailing garbage was rejected"

exit 0
