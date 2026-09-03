#!/bin/sh
# Preserve SIXEL_PALETTE_COVER equivalence with -a during migration.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"
cli_output="${ARTIFACT_LOCAL_DIR}/cover-policy-cli-$$.six"
env_output="${ARTIFACT_LOCAL_DIR}/cover-policy-env-$$.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -p 16 -Qheckbert -a corners \
    "${input}" >"${cli_output}" || {
    echo "not ok 1 - cover policy CLI conversion failed"
    exit 0
}
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -p 16 -Qheckbert \
    --env SIXEL_PALETTE_COVER=corners "${input}" >"${env_output}" || {
    echo "not ok 1 - cover policy environment conversion failed"
    exit 0
}
cmp -s "${cli_output}" "${env_output}" || {
    echo "not ok 1 - cover policy environment and CLI differ"
    exit 0
}

echo "ok 1 - cover policy environment matches CLI"
exit 0
