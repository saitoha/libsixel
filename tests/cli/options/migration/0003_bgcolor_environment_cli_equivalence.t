#!/bin/sh
# Preserve SIXEL_BGCOLOR equivalence with -B during migration.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input="${TOP_SRCDIR}/tests/data/inputs/formats/rgba.png"
cli_output="${ARTIFACT_LOCAL_DIR}/bgcolor-cli-$$.six"
env_output="${ARTIFACT_LOCAL_DIR}/bgcolor-env-$$.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin! -B '#123456' \
    "${input}" >"${cli_output}" || {
    echo "not ok 1 - bgcolor CLI conversion failed"
    exit 0
}
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin! \
    --env SIXEL_BGCOLOR=#123456 "${input}" >"${env_output}" || {
    echo "not ok 1 - bgcolor environment conversion failed"
    exit 0
}
cmp -s "${cli_output}" "${env_output}" || {
    echo "not ok 1 - bgcolor environment and CLI differ"
    exit 0
}

echo "ok 1 - bgcolor environment matches CLI"
exit 0
