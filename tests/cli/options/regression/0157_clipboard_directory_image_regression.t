#!/bin/sh
# Verify clipboard directory through short and environment paths.
# Registry row: SIXEL_OPTION_SCHEMA_CLIPBOARD_POLICY|g_clipboard_policy_values + SIXEL_CLIPBOARD_BASE_FILE|directory
# Registry binding: directory|directory_override
# Clipboard contract: backend=file|directory=1

set -eux

test "${HAVE_SIXEL2PNG-}" = 1 || {
    echo "1..0 # SKIP sixel2png is disabled in this build"
    exit 0
}
test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"
input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
short_dir="${artifact_dir}/0157-clipboard-directory-short-$$.d"
env_dir="${artifact_dir}/0157-clipboard-directory-env-$$.d"
short_output="${artifact_dir}/0157-clipboard-directory-short-$$.png"
env_output="${artifact_dir}/0157-clipboard-directory-env-$$.png"
mkdir -p "${short_dir}" "${env_dir}"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -y"file:D${short_dir}" "${input_image}" \
    -o clipboard: || {
    echo "not ok" 1 - "clipboard directory short write failed"
    exit 0
}
short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,clipboard_contract \
    ${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" -y"file:D${short_dir}" \
    -i clipboard: 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "clipboard directory short read failed"
    exit 0
}
test "${short_trace#*LSXSUB1|*key=directory|stored=1|binding=directory,directory_override|value="${short_dir}"*}" != "${short_trace}" || {
    echo "not ok" 1 - "clipboard directory short value was not stored"
    exit 0
}
test "${short_trace#*LSXCLP1|*backend=file|directory=1*}" != \
    "${short_trace}" || {
    echo "not ok" 1 - "clipboard directory short value missed its consumer"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_CLIPBOARD_BACKEND=file \
    --env "SIXEL_CLIPBOARD_FILE_DIR=${env_dir}" \
    "${input_image}" -o clipboard: || {
    echo "not ok" 1 - "clipboard directory environment write failed"
    exit 0
}
env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,clipboard_contract \
    ${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" \
    --env "SIXEL_CLIPBOARD_FILE_DIR=${env_dir}" -yfile \
    -i clipboard: 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "clipboard directory environment read failed"
    exit 0
}
test "${env_trace#*LSXSUB1|*key=directory|stored=1|binding=directory,directory_override|value="${env_dir}"*}" != "${env_trace}" || {
    echo "not ok" 1 - "clipboard directory environment value was not stored"
    exit 0
}
test "${env_trace#*LSXCLP1|*backend=file|directory=1*}" != \
    "${env_trace}" || {
    echo "not ok" 1 - "clipboard directory environment missed its consumer"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "clipboard directory outputs differ"
    exit 0
}
lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -b "MS-SSIM:0.98" "${input_image}" "${short_output}" 2>&1) || \
    lsqa_status=$?
test "${lsqa_status:-0}" -eq 0 || {
    echo "not ok" 1 - "clipboard directory image quality regressed"
    printf "# %s\n" "${lsqa_error}"
    exit 0
}

echo "ok" 1 - "clipboard directory preserves decoded image output"
exit 0
