#!/bin/sh
# Preserve SIXEL_UPDATE_DELTA_ERROR equivalence with delta:error.
# Policy: docs/functionality/delta-encoding.md

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input="${TOP_SRCDIR}/tests/data/inputs/formats/apng_8x8_rgba_loop2.png"
cli_output="${ARTIFACT_LOCAL_DIR}/update-delta-error-cli-$$.six"
env_output="${ARTIFACT_LOCAL_DIR}/update-delta-error-env-$$.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -l disable -Z delta:threshold=32:error=skip \
    "${input}" >"${cli_output}" || {
    echo "not ok 1 - 6delta error CLI conversion failed"
    exit 0
}
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -l disable -Z delta:threshold=32 \
    --env SIXEL_UPDATE_DELTA_ERROR=skip "${input}" >"${env_output}" || {
    echo "not ok 1 - 6delta error environment conversion failed"
    exit 0
}
cmp -s "${cli_output}" "${env_output}" || {
    echo "not ok 1 - 6delta error environment and CLI differ"
    exit 0
}

echo "ok 1 - 6delta error environment matches CLI"
exit 0
