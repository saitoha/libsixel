#!/bin/sh
# Preserve SIXEL_COLORS equivalence with -p during migration.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"
cli_output="${ARTIFACT_LOCAL_DIR}/colors-cli-$$.six"
env_output="${ARTIFACT_LOCAL_DIR}/colors-env-$$.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -p 16 "${input}" \
    >"${cli_output}" || {
    echo "not ok 1 - colors CLI conversion failed"
    exit 0
}
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_COLORS=16 "${input}" \
    >"${env_output}" || {
    echo "not ok 1 - colors environment conversion failed"
    exit 0
}
cmp -s "${cli_output}" "${env_output}" || {
    echo "not ok 1 - colors environment and CLI differ"
    exit 0
}

echo "ok 1 - colors environment matches CLI"
exit 0
