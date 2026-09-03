#!/bin/sh
# Preserve start-frame environment equivalence with -T during migration.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input="${TOP_SRCDIR}/tests/data/inputs/formats/apng_8x8_rgba_loop2.png"
cli_output="${ARTIFACT_LOCAL_DIR}/start-frame-cli-$$.six"
env_output="${ARTIFACT_LOCAL_DIR}/start-frame-env-$$.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -S -L builtin! -T 1 "${input}" \
    >"${cli_output}" || {
    echo "not ok 1 - start-frame CLI conversion failed"
    exit 0
}
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -S -L builtin! \
    --env SIXEL_LOADER_ANIMATION_START_FRAME_NO=1 "${input}" \
    >"${env_output}" || {
    echo "not ok 1 - start-frame environment conversion failed"
    exit 0
}
cmp -s "${cli_output}" "${env_output}" || {
    echo "not ok 1 - start-frame environment and CLI differ"
    exit 0
}

echo "ok 1 - start-frame environment matches CLI"
exit 0
