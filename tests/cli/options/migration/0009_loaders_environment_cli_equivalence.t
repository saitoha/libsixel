#!/bin/sh
# Preserve SIXEL_LOADER_PRIORITY_LIST equivalence with -L during migration.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input="${TOP_SRCDIR}/tests/data/inputs/formats/rgba.png"
cli_output="${ARTIFACT_LOCAL_DIR}/loaders-cli-$$.six"
env_output="${ARTIFACT_LOCAL_DIR}/loaders-env-$$.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin! "${input}" \
    >"${cli_output}" || {
    echo "not ok 1 - loaders CLI conversion failed"
    exit 0
}
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_LOADER_PRIORITY_LIST=builtin! "${input}" \
    >"${env_output}" || {
    echo "not ok 1 - loaders environment conversion failed"
    exit 0
}
cmp -s "${cli_output}" "${env_output}" || {
    echo "not ok 1 - loaders environment and CLI differ"
    exit 0
}

echo "ok 1 - loaders environment matches CLI"
exit 0
