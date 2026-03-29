#!/bin/sh
# Verify parse error when -b is specified more than once.

set -eux


printf '1..1\n'
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

image_ref="${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp"
image_out="${TOP_SRCDIR}/tests/data/inputs/snake_64.six"
err_file="${ARTIFACT_LOCAL_DIR}/lsqa_duplicate_baseline.err"

set +e
${SIXEL_RUNTIME-} "${LSQA_PATH}" -b "MS-SSIM:0.0" -b "MS-SSIM:0.0" "${image_ref}" "${image_out}" >"/dev/null" 2>"${err_file}"
status=$?
set -e

test "${status}" -eq 2 || {
    echo "not ok" 1 - "duplicate -b option was not rejected as expected"
    exit 0
}

err_match=0
while IFS= read -r line; do
    case "${line}" in
        *"baseline already specified"*)
            err_match=1
            break
            ;;
    esac
done < "${err_file}"

test "${err_match}" -eq 1 || {
    echo "not ok" 1 - "duplicate -b option was not rejected as expected"
    exit 0
}

echo "ok" 1 - "duplicate -b option was rejected"

exit 0
