#!/bin/sh
# Preserve SIXEL_GPU_POLICY equivalence with -G during migration.

set -eux

test "${HAVE_SIXEL2PNG-}" = 1 || {
    printf "1..0 # SKIP sixel2png is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input="${TOP_SRCDIR}/images/map64.six"
cli_output="${ARTIFACT_LOCAL_DIR}/gpu-policy-cli-$$.png"
env_output="${ARTIFACT_LOCAL_DIR}/gpu-policy-env-$$.png"

${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" -G off <"${input}" \
    >"${cli_output}" || {
    echo "not ok 1 - GPU policy CLI conversion failed"
    exit 0
}
${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" --env SIXEL_GPU_POLICY=off \
    <"${input}" >"${env_output}" || {
    echo "not ok 1 - GPU policy environment conversion failed"
    exit 0
}
cmp -s "${cli_output}" "${env_output}" || {
    echo "not ok 1 - GPU policy environment and CLI differ"
    exit 0
}

echo "ok 1 - GPU policy environment matches CLI"
exit 0
