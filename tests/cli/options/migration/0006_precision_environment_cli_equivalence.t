#!/bin/sh
# Preserve SIXEL_FLOAT32_DITHER equivalence with -. during migration.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
cli_output="${ARTIFACT_LOCAL_DIR}/precision-cli-$$.six"
env_output="${ARTIFACT_LOCAL_DIR}/precision-env-$$.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -. float32 "${input}" \
    >"${cli_output}" || {
    echo "not ok 1 - precision CLI conversion failed"
    exit 0
}
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_FLOAT32_DITHER=1 \
    "${input}" >"${env_output}" || {
    echo "not ok 1 - precision environment conversion failed"
    exit 0
}
cmp -s "${cli_output}" "${env_output}" || {
    echo "not ok 1 - precision environment and CLI differ"
    exit 0
}

echo "ok 1 - precision environment matches CLI"
exit 0
